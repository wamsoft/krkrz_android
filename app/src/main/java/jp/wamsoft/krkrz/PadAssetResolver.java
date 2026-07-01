package jp.wamsoft.krkrz;

import android.app.Activity;
import android.content.Context;
import android.system.ErrnoException;
import android.system.Os;
import android.util.Log;

import com.google.android.play.core.assetpacks.AssetPackLocation;
import com.google.android.play.core.assetpacks.AssetPackManager;
import com.google.android.play.core.assetpacks.AssetPackManagerFactory;
import com.google.android.play.core.assetpacks.AssetPackState;
import com.google.android.play.core.assetpacks.AssetPackStateUpdateListener;
import com.google.android.play.core.assetpacks.model.AssetPackStatus;

import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.LinkOption;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Play Asset Delivery (PAD) 用のランタイム解決ヘルパ。
 *
 * fast-follow / on-demand パックは内部ストレージに展開済み実ファイルとして置かれ、
 * {@link AssetPackManager#getPackLocation} から絶対パス ({@link AssetPackLocation#assetsPath})
 * が取れる。これを既存の {@code setAssetCacheDir()} に渡すと、エンジンの高速な
 * 絶対パス→filesystem 経路がそのまま効く（コピー不要・native 無改修）。
 *
 * install-time パックは実ファイル展開されず AssetManager 名前空間に出るため、
 * こちらは対象外（{@link BootstrapActivity} の assets コピー経路が拾う）。ここで扱うのは
 * ビルド時に生成される {@code R.array.pad_fetch_packs}（= fast-follow + on-demand）のみ。
 *
 * 複数パックの {@code assetsPath()} を、エンジンが要求する単一ルートに束ねるため
 * {@code <filesDir>/padroot/} に **毎起動 symlink farm を作り直す**（パスはキャッシュ禁止）。
 */
final class PadAssetResolver {

    private static final String TAG = "PadAssetResolver";

    /** ダウンロード進捗コールバック。byte 単位（GB になりうるので long）。 */
    interface Progress {
        void update(String label, long bytesDone, long bytesTotal);
    }

    private PadAssetResolver() {}

    /**
     * 指定パックを全て COMPLETED まで用意し（未取得は fetch して進捗表示）、
     * 各パックの assets 絶対パスを返す。1 つでも用意できなければ例外。
     * ブロッキング。呼び出しはワーカースレッド想定（PAD コールバックは main に来る）。
     */
    static List<String> ensurePacksReady(Activity activity, String[] packNames, Progress progress)
            throws IOException, InterruptedException {
        final AssetPackManager mgr =
                AssetPackManagerFactory.getInstance(activity.getApplicationContext());

        // 進捗集計用（name -> [done,total]）。順序維持でログを読みやすく。
        final Map<String, long[]> prog = Collections.synchronizedMap(new LinkedHashMap<>());
        // まだ COMPLETED でないパック集合。
        final Set<String> pending = Collections.synchronizedSet(new HashSet<>(Arrays.asList(packNames)));

        // 既に展開済み（前回起動でDL済み等）のものは pending から外す。
        for (String name : packNames) {
            AssetPackLocation loc = mgr.getPackLocation(name);
            if (loc != null && loc.assetsPath() != null) {
                pending.remove(name);
            }
        }

        if (!pending.isEmpty()) {
            final CountDownLatch latch = new CountDownLatch(1);
            final AtomicReference<String> failure = new AtomicReference<>(null);

            AssetPackStateUpdateListener listener = new AssetPackStateUpdateListener() {
                @Override
                public void onStateUpdate(AssetPackState state) {
                    String name = state.name();
                    prog.put(name, new long[]{ state.bytesDownloaded(), state.totalBytesToDownload() });
                    long done = 0, total = 0;
                    synchronized (prog) {
                        for (long[] v : prog.values()) { done += v[0]; total += v[1]; }
                    }
                    progress.update(name + " " + statusLabel(state.status()), done, total);

                    switch (state.status()) {
                        case AssetPackStatus.COMPLETED:
                            pending.remove(name);
                            if (pending.isEmpty()) latch.countDown();
                            break;
                        case AssetPackStatus.FAILED:
                            failure.set("pack '" + name + "' failed: errorCode=" + state.errorCode());
                            latch.countDown();
                            break;
                        case AssetPackStatus.CANCELED:
                            failure.set("pack '" + name + "' canceled");
                            latch.countDown();
                            break;
                        case AssetPackStatus.WAITING_FOR_WIFI:
                        case AssetPackStatus.REQUIRES_USER_CONFIRMATION:
                            // 大容量セルラー確認 / Wi-Fi 待ちはユーザー確認ダイアログを出す。
                            mgr.showConfirmationDialog(activity);
                            break;
                        default:
                            break;
                    }
                }
            };

            mgr.registerListener(listener);
            try {
                List<String> toFetch = new ArrayList<>(pending);
                Log.i(TAG, "fetching PAD packs: " + toFetch);
                mgr.fetch(toFetch);
                latch.await();
                String err = failure.get();
                if (err != null) {
                    throw new IOException(err);
                }
            } finally {
                mgr.unregisterListener(listener);
            }
        }

        // 全パックの assets 絶対パスを収集。
        List<String> dirs = new ArrayList<>();
        for (String name : packNames) {
            AssetPackLocation loc = mgr.getPackLocation(name);
            if (loc == null || loc.assetsPath() == null) {
                throw new IOException("PAD pack not ready after fetch: " + name);
            }
            String assetsPath = loc.assetsPath();
            Log.i(TAG, "pack '" + name + "' assetsPath=" + assetsPath);
            dirs.add(assetsPath);
        }
        return dirs;
    }

    /**
     * 複数のルートディレクトリを 1 つの {@code padroot} に symlink で束ねる（コピー無し）。
     * {@code roots} は優先順（先勝ち）。毎回作り直す。返り値は padroot（末尾スラッシュ無し）。
     *
     * 同 UID の内部ストレージ内なので SDL3 の絶対パス open がそのまま追従する。
     */
    static File buildSymlinkFarm(File padroot, List<File> roots) throws IOException {
        deleteRecursively(padroot);
        if (!padroot.mkdirs() && !padroot.isDirectory()) {
            throw new IOException("failed to create padroot: " + padroot);
        }
        for (File root : roots) {
            if (root == null || !root.isDirectory()) continue;
            linkTree(root, root, padroot);
        }
        return padroot;
    }

    /** {@code base} 起点の相対パスを保ったまま {@code src} 以下のファイルを padroot へ symlink（先勝ち）。 */
    private static void linkTree(File base, File src, File padroot) throws IOException {
        File[] children = src.listFiles();
        if (children == null) return;
        for (File child : children) {
            if (child.isDirectory()) {
                linkTree(base, child, padroot);
            } else {
                String rel = base.toPath().relativize(child.toPath()).toString().replace('\\', '/');
                File link = new File(padroot, rel);
                // first wins. Use NOFOLLOW so an existing (even dangling) symlink counts as present.
                if (Files.exists(link.toPath(), LinkOption.NOFOLLOW_LINKS)) continue;
                File parent = link.getParentFile();
                if (parent != null && !parent.isDirectory() && !parent.mkdirs() && !parent.isDirectory()) {
                    throw new IOException("failed to create dir: " + parent);
                }
                try {
                    Os.symlink(child.getAbsolutePath(), link.getAbsolutePath());
                } catch (ErrnoException e) {
                    throw new IOException("symlink failed: " + link + " -> " + child, e);
                }
            }
        }
    }

    /**
     * padroot を作り直す前に丸ごと消す。symlink farm 特有の注意として、リンク先が既に消えた
     * **ぶら下がり symlink** を確実に削除する必要がある。{@link File#exists()} は symlink を
     * 辿るためぶら下がりを「存在しない」と誤判定してしまうので、ここでは NOFOLLOW で判定し、
     * ディレクトリ実体だけ再帰する（symlink 先は辿らない）。
     */
    private static void deleteRecursively(File path) {
        if (path == null) return;
        java.nio.file.Path p = path.toPath();
        boolean isSymlink = Files.isSymbolicLink(p);
        if (!isSymlink && !Files.exists(p, LinkOption.NOFOLLOW_LINKS)) return;
        // 実ディレクトリのみ再帰（symlink はディレクトリでも辿らずリンク自体を消す）。
        if (!isSymlink && Files.isDirectory(p, LinkOption.NOFOLLOW_LINKS)) {
            File[] children = path.listFiles();
            if (children != null) {
                for (File c : children) deleteRecursively(c);
            }
        }
        try {
            Files.deleteIfExists(p);
        } catch (IOException e) {
            // best-effort: 消せなくても後続の mkdirs / symlink 側で顕在化する
            Log.w(TAG, "delete failed: " + path, e);
        }
    }

    private static String statusLabel(int status) {
        switch (status) {
            case AssetPackStatus.PENDING: return "pending";
            case AssetPackStatus.DOWNLOADING: return "downloading";
            case AssetPackStatus.TRANSFERRING: return "installing";
            case AssetPackStatus.COMPLETED: return "completed";
            case AssetPackStatus.FAILED: return "failed";
            case AssetPackStatus.CANCELED: return "canceled";
            case AssetPackStatus.WAITING_FOR_WIFI: return "waiting for Wi-Fi";
            case AssetPackStatus.NOT_INSTALLED: return "not installed";
            case AssetPackStatus.REQUIRES_USER_CONFIRMATION: return "needs confirmation";
            default: return "…";
        }
    }
}
