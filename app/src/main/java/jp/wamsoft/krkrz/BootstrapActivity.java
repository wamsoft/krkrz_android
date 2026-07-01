package jp.wamsoft.krkrz;

import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageInfo;
import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.DisplayMetrics;
import android.util.Log;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.TextView;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;

/**
 * KrkrZ 本体起動前に走るブートストラップ Activity。
 *
 * Android の AssetManager 経由で xp3 アーカイブを読むのは遅い (圧縮されてないが
 * AAsset_read が seek 系で重い + アーカイブ全体を一度メモリに展開する実装の都合)。
 * このため起動時に一度 assets/ 内の *.xp3 (再帰) を内部ストレージ
 * `<filesDir>/{@link #ASSET_CACHE_SUBDIR}/` にコピーしておき、
 * 以後は通常のファイルシステム経由でアクセスする。
 *
 * 再コピー判定は APK の (versionCode, lastUpdateTime) を marker に保存して比較する
 * 一括判定方式。これらが一致すれば「APK 更新されてない」とみなしてスキップする。
 * mismatch 時は cache ディレクトリを丸ごとリセットしてから全コピーする。
 *
 * 完了後は {@link KrkrzActivity} を startActivity して finish() する。
 * KrkrzActivity 側でも同じ `<filesDir>/{@link #ASSET_CACHE_SUBDIR}/` を組み立てて
 * native (stdapp.cpp::InitPath) に渡す (経路: setAssetCacheDir JNI)。
 */
public class BootstrapActivity extends Activity {

    private static final String TAG = "BootstrapActivity";

    /** {@link android.content.Context#getFilesDir()} 配下に作る cache サブディレクトリ名。 */
    public static final String ASSET_CACHE_SUBDIR = "assets";

    /**
     * PAD パックを複数束ねる symlink farm のサブディレクトリ名（{@code <filesDir>/padroot/}）。
     * fast-follow / on-demand パックの {@code assetsPath()} を単一ルートに統合する。
     */
    public static final String PAD_ROOT_SUBDIR = "padroot";

    private static final String PREFS_NAME = "bootstrap";
    private static final String KEY_VERSION_CODE = "apk_version_code";
    private static final String KEY_LAST_UPDATE_TIME = "apk_last_update_time";
    /** 1 回の I/O バッファ。xp3 は数百 MB あるので大きめに取る。 */
    private static final int COPY_BUFFER_BYTES = 1 << 20; // 1 MiB

    /** ローディング背景に使う assets 直下の画像候補 (先に見つかったものを使用)。 */
    private static final String[] BACKGROUND_ASSET_CANDIDATES = { "loading.jpg", "loading.png" };

    private TextView statusText;
    private ProgressBar progressBar;
    private ImageView backgroundImage;
    private final Handler mainHandler = new Handler(Looper.getMainLooper());

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // KrkrzActivity (SDL) と同じ向きにローディング画面も合わせる。値は
        // R.string.sdl_orientations (resOverride で差し替え可)。SDL の hint 解釈に合わせて
        // ActivityInfo の向きへマップする。
        setRequestedOrientation(orientationFromSdlHint(getString(R.string.sdl_orientations)));

        setContentView(R.layout.bootstrap);

        statusText = findViewById(R.id.bootstrap_status);
        progressBar = findViewById(R.id.bootstrap_progress);
        backgroundImage = findViewById(R.id.bootstrap_bg);

        loadBackgroundAsync();
        new Thread(this::runCopyThenLaunch, "BootstrapCopy").start();
    }

    /**
     * ローディング背景画像を assets から非同期に読み込んで表示する。
     * {@link #BACKGROUND_ASSET_CANDIDATES} を順に探し、最初に見つかったものを
     * 画面サイズに合わせてダウンサンプルして {@code bootstrap_bg} にセットする。
     * 画像が無い案件 (sample 等) では何もせず黒背景のまま。
     */
    private void loadBackgroundAsync() {
        new Thread(() -> {
            final Bitmap bmp = decodeAssetBackground();
            if (bmp != null) {
                mainHandler.post(() -> {
                    if (backgroundImage != null) backgroundImage.setImageBitmap(bmp);
                });
            }
        }, "BootstrapBg").start();
    }

    private Bitmap decodeAssetBackground() {
        AssetManager am = getAssets();
        DisplayMetrics dm = getResources().getDisplayMetrics();
        int reqW = Math.max(1, dm.widthPixels);
        int reqH = Math.max(1, dm.heightPixels);
        for (String name : BACKGROUND_ASSET_CANDIDATES) {
            try {
                BitmapFactory.Options bounds = new BitmapFactory.Options();
                bounds.inJustDecodeBounds = true;
                try (InputStream in = am.open(name)) {
                    BitmapFactory.decodeStream(in, null, bounds);
                }
                if (bounds.outWidth <= 0 || bounds.outHeight <= 0) continue;

                BitmapFactory.Options opts = new BitmapFactory.Options();
                opts.inSampleSize = calcInSampleSize(bounds.outWidth, bounds.outHeight, reqW, reqH);
                try (InputStream in = am.open(name)) {
                    Bitmap bmp = BitmapFactory.decodeStream(in, null, opts);
                    if (bmp != null) {
                        Log.i(TAG, "loading background = " + name
                                + " (" + bmp.getWidth() + "x" + bmp.getHeight() + ")");
                        return bmp;
                    }
                }
            } catch (IOException e) {
                // 見つからない候補は無視して次へ
            }
        }
        return null; // 無ければ黒背景のまま
    }

    /** centerCrop 品質を保ちつつメモリを抑えるダウンサンプル係数 (画面より少し大きめで止める)。 */
    private static int calcInSampleSize(int srcW, int srcH, int reqW, int reqH) {
        int sample = 1;
        while ((srcH / (sample * 2)) >= reqH && (srcW / (sample * 2)) >= reqW) {
            sample *= 2;
        }
        return sample;
    }

    /**
     * SDL_HINT_ORIENTATIONS の値 (空白区切りの LandscapeLeft/LandscapeRight/Portrait/
     * PortraitUpsideDown) を Activity の requestedOrientation にマップする。
     * SDL の setOrientationBis と同じ意味付け (両向き→USER_*、片方→固定)。
     */
    private static int orientationFromSdlHint(String hint) {
        if (hint == null || hint.trim().isEmpty()) {
            return ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;
        }
        boolean ll = false, lr = false, p = false, pu = false;
        for (String t : hint.trim().split("\\s+")) {
            switch (t) {
                case "LandscapeLeft":       ll = true; break;
                case "LandscapeRight":      lr = true; break;
                case "Portrait":            p = true;  break;
                case "PortraitUpsideDown":  pu = true; break;
                default: break;
            }
        }
        boolean land = ll || lr, port = p || pu;
        if (land && port) return ActivityInfo.SCREEN_ORIENTATION_FULL_USER;
        if (ll && lr)     return ActivityInfo.SCREEN_ORIENTATION_USER_LANDSCAPE;
        if (p && pu)      return ActivityInfo.SCREEN_ORIENTATION_USER_PORTRAIT;
        if (ll)           return ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE;
        if (lr)           return ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE;
        if (p)            return ActivityInfo.SCREEN_ORIENTATION_PORTRAIT;
        if (pu)           return ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT;
        return ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;
    }

    private void runCopyThenLaunch() {
        try {
            ensureAssetsCached();
        } catch (Exception e) {
            Log.e(TAG, "asset cache failed", e);
            // 失敗しても KrkrzActivity 起動は試みる (engine が AssetManager に
            // フォールバックする経路を持っているため、最悪 legacy ルートで動く)。
        }

        // データルートを優先順で解決:
        //   1. PAD (fast-follow/on-demand) パック → symlink farm で束ねた padroot
        //   2. 従来の assets コピー cache (<filesDir>/assets/)  ← install-time パック含む
        //   3. legacy (null → engine が AssetManager フォールバック)
        String assetRoot = null;
        try {
            assetRoot = resolveAssetRoot();
        } catch (Exception e) {
            Log.e(TAG, "PAD resolve failed; falling back to assets cache", e);
        }
        if (assetRoot == null) {
            File assetsCache = new File(getFilesDir(), ASSET_CACHE_SUBDIR);
            if (assetsCache.isDirectory()) {
                assetRoot = assetsCache.getAbsolutePath() + "/";
            }
        }

        final String finalRoot = assetRoot;
        mainHandler.post(() -> launchMainActivity(finalRoot));
    }

    /**
     * PAD の fetch パック (R.array.pad_fetch_packs = fast-follow + on-demand) を用意し、
     * assets cache と合わせて {@code <filesDir>/padroot/} に symlink 統合したパスを返す。
     * fetch パックが無ければ従来の assets cache（または null）を返す。
     */
    private String resolveAssetRoot() throws IOException, InterruptedException {
        String[] fetchPacks = getResources().getStringArray(R.array.pad_fetch_packs);
        File assetsCache = new File(getFilesDir(), ASSET_CACHE_SUBDIR);

        if (fetchPacks.length == 0) {
            // PAD 無し（sample 等）: 従来どおり assets cache を単一ルートに。
            return assetsCache.isDirectory() ? assetsCache.getAbsolutePath() + "/" : null;
        }

        postStatus("Preparing add-on data…", 0, 1);
        List<String> packDirs = PadAssetResolver.ensurePacksReady(this, fetchPacks, this::postProgressBytes);

        List<File> roots = new ArrayList<>();
        // 先勝ち: base assets (install-time パック含む) を優先、続いて各 PAD パック。
        if (assetsCache.isDirectory()) roots.add(assetsCache);
        for (String d : packDirs) roots.add(new File(d));

        File padroot = new File(getFilesDir(), PAD_ROOT_SUBDIR);
        PadAssetResolver.buildSymlinkFarm(padroot, roots);
        Log.i(TAG, "asset root (PAD merged) = " + padroot.getAbsolutePath());
        return padroot.getAbsolutePath() + "/";
    }

    private void launchMainActivity(String assetRoot) {
        Intent intent = new Intent(this, KrkrzActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
        if (assetRoot != null) {
            intent.putExtra(KrkrzActivity.EXTRA_ASSET_ROOT, assetRoot);
        }
        startActivity(intent);
        finish();
    }

    // ----- 以下、コピー処理本体 -----

    private void ensureAssetsCached() throws IOException {
        File cacheRoot = new File(getFilesDir(), ASSET_CACHE_SUBDIR);
        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);

        long curVersion = currentVersionCode();
        long curUpdate = currentLastUpdateTime();
        long prevVersion = prefs.getLong(KEY_VERSION_CODE, -1L);
        long prevUpdate = prefs.getLong(KEY_LAST_UPDATE_TIME, -1L);

        boolean upToDate = cacheRoot.isDirectory()
                && curVersion == prevVersion
                && curUpdate == prevUpdate;

        if (upToDate) {
            Log.i(TAG, "asset cache up to date (versionCode=" + curVersion
                    + ", lastUpdateTime=" + curUpdate + ")");
            postStatus("Ready", 1, 1);
            return;
        }

        Log.i(TAG, "asset cache stale; rebuilding (cur=" + curVersion + "/" + curUpdate
                + " prev=" + prevVersion + "/" + prevUpdate + ")");
        postStatus("Preparing cache...", 0, 1);
        deleteRecursively(cacheRoot);
        if (!cacheRoot.mkdirs() && !cacheRoot.isDirectory()) {
            throw new IOException("failed to create cache dir: " + cacheRoot);
        }

        // 対象ファイルを列挙: (1) 任意階層の *.xp3、(2) data/ 配下の全ファイル。
        // 小物 (config.cf / fonts / SysInitScript.tjs / messages*.json 等) は engine の
        // `resource://` 経由で AssetManager 直アクセスのままなのでコピー対象外。
        AssetManager am = getAssets();
        List<String> targets = new ArrayList<>();
        collectCacheTargets(am, "", targets);
        Log.i(TAG, "cache targets: " + targets.size());

        int total = targets.size();
        for (int i = 0; i < total; i++) {
            String rel = targets.get(i);
            postStatus(rel, i, total);
            copyAsset(am, rel, new File(cacheRoot, rel));
        }
        postStatus("Done", total, total);

        prefs.edit()
                .putLong(KEY_VERSION_CODE, curVersion)
                .putLong(KEY_LAST_UPDATE_TIME, curUpdate)
                .apply();
    }

    /**
     * assets 配下を再帰的に走査してキャッシュ対象を集める。relative path (assets root 起点)。
     * 採用ルール:
     *  - 任意階層の {@code *.xp3}
     *  - {@code data/} 配下の全ファイル (video/ 等)
     *
     * data/ 配下を丸ごと拾うのは、KrkrZ engine の autopath や Video 再生が
     * project path 起点 (= cache dir) から直接 file を引きに来るため。
     * 例えば {@code data/video/intro.mp4} は engine が
     * {@code <cache>/data/video/intro.mp4} として open しに行く。
     *
     * フォルダ判定は {@link AssetManager#list(String)} の戻りが空かどうかで行う
     * (Android の AssetManager にはディレクトリ/ファイルの直接判別 API がない)。
     */
    private void collectCacheTargets(AssetManager am, String dir, List<String> out) throws IOException {
        String[] entries = am.list(dir);
        if (entries == null) return;
        for (String entry : entries) {
            String rel = dir.isEmpty() ? entry : dir + "/" + entry;
            String[] sub = am.list(rel);
            boolean isDir = (sub != null && sub.length > 0);
            if (isDir) {
                collectCacheTargets(am, rel, out);
            } else {
                if (entry.endsWith(".xp3") || isUnderDataFolder(rel)) {
                    out.add(rel);
                }
            }
        }
    }

    /** {@code data/...} 形式 (assets root 直下の data/ 配下) のとき true。 */
    private static boolean isUnderDataFolder(String rel) {
        return rel.startsWith("data/") || rel.equals("data");
    }

    private void copyAsset(AssetManager am, String relPath, File dst) throws IOException {
        File parent = dst.getParentFile();
        if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
            throw new IOException("failed to create parent dir: " + parent);
        }
        try (InputStream in = am.open(relPath, AssetManager.ACCESS_STREAMING);
             FileOutputStream out = new FileOutputStream(dst)) {
            byte[] buf = new byte[COPY_BUFFER_BYTES];
            int n;
            while ((n = in.read(buf)) > 0) {
                out.write(buf, 0, n);
            }
        }
    }

    private void deleteRecursively(File path) {
        if (path == null || !path.exists()) return;
        if (path.isDirectory()) {
            File[] children = path.listFiles();
            if (children != null) {
                for (File c : children) deleteRecursively(c);
            }
        }
        // best-effort: delete 失敗しても次の mkdirs で上書きされるので致命ではない
        //noinspection ResultOfMethodCallIgnored
        path.delete();
    }

    private long currentVersionCode() {
        try {
            PackageInfo info = getPackageManager().getPackageInfo(getPackageName(), 0);
            return android.os.Build.VERSION.SDK_INT >= 28
                    ? info.getLongVersionCode()
                    : info.versionCode;
        } catch (Exception e) {
            return -1L;
        }
    }

    private long currentLastUpdateTime() {
        try {
            PackageInfo info = getPackageManager().getPackageInfo(getPackageName(), 0);
            return info.lastUpdateTime;
        } catch (Exception e) {
            return -1L;
        }
    }

    /** PAD ダウンロード進捗（byte 単位、GB になりうる）を MB スケールで表示。 */
    private void postProgressBytes(String label, long done, long total) {
        mainHandler.post(() -> {
            int doneMb = (int) (done / (1024L * 1024L));
            int totalMb = (int) Math.max(1L, total / (1024L * 1024L));
            if (statusText != null) {
                statusText.setText(getString(R.string.bootstrap_status_mb_fmt, label, doneMb, totalMb));
            }
            if (progressBar != null) {
                progressBar.setMax(totalMb);
                progressBar.setProgress(doneMb);
            }
        });
    }

    private void postStatus(String label, int done, int total) {
        mainHandler.post(() -> {
            if (statusText != null) {
                statusText.setText(getString(R.string.bootstrap_status_fmt, label, done, total));
            }
            if (progressBar != null) {
                progressBar.setMax(Math.max(1, total));
                progressBar.setProgress(done);
            }
        });
    }
}
