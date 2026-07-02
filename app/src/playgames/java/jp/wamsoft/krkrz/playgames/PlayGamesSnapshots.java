package jp.wamsoft.krkrz.playgames;

import android.app.Activity;
import android.content.Intent;

import com.google.android.gms.games.PlayGames;
import com.google.android.gms.games.SnapshotsClient;
import com.google.android.gms.games.snapshot.Snapshot;
import com.google.android.gms.games.snapshot.SnapshotMetadata;
import com.google.android.gms.games.snapshot.SnapshotMetadataChange;
import com.google.android.gms.games.snapshot.SnapshotMetadataBuffer;

import java.util.HashMap;

/**
 * セーブゲーム (Snapshots) サブシステム。
 *
 * Steam Cloud と異なり Snapshots は1スナップショット=1ファイル単位で扱う
 * 名前付きスロット型。read/write は競合解決ポリシー必須。
 *
 * 競合解決ポリシーは原則 RESOLUTION_POLICY_MOST_RECENTLY_MODIFIED を使う。
 */
public final class PlayGamesSnapshots {
    private PlayGamesSnapshots() {}

    /** スナップショット一覧を取得。結果は Object[] of HashMap (uniqueName, description, lastModifiedTimestamp, deviceName, coverImageAspectRatio, totalPlayedTime, progressValue)。 */
    public static void list(final long callbackId, final boolean forceReload) {
        final Activity activity = PlayGamesBridge.activity();
        if (activity == null) {
            PlayGamesBridge.postError(callbackId, "PlayGamesBridge not initialized");
            return;
        }
        try {
            SnapshotsClient client = PlayGames.getSnapshotsClient(activity);
            client.load(forceReload).addOnCompleteListener(task -> {
                if (!task.isSuccessful() || task.getResult() == null) {
                    PlayGamesBridge.postError(callbackId,
                            PlayGamesBridge.exceptionMessage(task.getException()));
                    return;
                }
                SnapshotMetadataBuffer buffer = task.getResult().get();
                try {
                    if (buffer == null) {
                        PlayGamesBridge.postResult(callbackId, new Object[0]);
                        return;
                    }
                    int n = buffer.getCount();
                    Object[] arr = new Object[n];
                    for (int i = 0; i < n; ++i) {
                        SnapshotMetadata m = buffer.get(i);
                        arr[i] = metadataToDict(m);
                    }
                    PlayGamesBridge.postResult(callbackId, arr);
                } finally {
                    try { buffer.release(); } catch (Throwable ignore) {}
                }
            });
        } catch (Throwable t) {
            PlayGamesBridge.postError(callbackId, PlayGamesBridge.exceptionMessage(t));
        }
    }

    /**
     * 指定スナップショットを読み込む。結果は HashMap (metadata, data)。data は byte[] ->
     * Native 側で string/octet として扱える形に渡す。
     */
    public static void read(final long callbackId, final String uniqueName) {
        final Activity activity = PlayGamesBridge.activity();
        if (activity == null) {
            PlayGamesBridge.postError(callbackId, "PlayGamesBridge not initialized");
            return;
        }
        try {
            SnapshotsClient client = PlayGames.getSnapshotsClient(activity);
            client.open(uniqueName, false, SnapshotsClient.RESOLUTION_POLICY_MOST_RECENTLY_MODIFIED)
                    .addOnCompleteListener(task -> {
                        if (!task.isSuccessful() || task.getResult() == null
                                || task.getResult().getData() == null) {
                            PlayGamesBridge.postError(callbackId,
                                    PlayGamesBridge.exceptionMessage(task.getException()));
                            return;
                        }
                        Snapshot snapshot = task.getResult().getData();
                        try {
                            byte[] bytes = snapshot.getSnapshotContents().readFully();
                            HashMap<String, Object> result = PlayGamesNative.dict();
                            result.put("metadata", metadataToDict(snapshot.getMetadata()));
                            result.put("data", bytes);
                            PlayGamesBridge.postResult(callbackId, result);
                        } catch (Throwable t) {
                            PlayGamesBridge.postError(callbackId,
                                    PlayGamesBridge.exceptionMessage(t));
                        } finally {
                            // 競合解決後は discard する (書き込まないため)
                            try {
                                client.discardAndClose(snapshot);
                            } catch (Throwable ignore) {}
                        }
                    });
        } catch (Throwable t) {
            PlayGamesBridge.postError(callbackId, PlayGamesBridge.exceptionMessage(t));
        }
    }

    /**
     * 指定スナップショットへ書き込む。description / playedTimeMs / progressValue は省略可 (-1で無視)。
     */
    public static void write(final long callbackId,
                              final String uniqueName,
                              final byte[] data,
                              final String description,
                              final long playedTimeMs,
                              final long progressValue) {
        final Activity activity = PlayGamesBridge.activity();
        if (activity == null) {
            PlayGamesBridge.postError(callbackId, "PlayGamesBridge not initialized");
            return;
        }
        try {
            SnapshotsClient client = PlayGames.getSnapshotsClient(activity);
            client.open(uniqueName, true, SnapshotsClient.RESOLUTION_POLICY_MOST_RECENTLY_MODIFIED)
                    .addOnCompleteListener(task -> {
                        if (!task.isSuccessful() || task.getResult() == null
                                || task.getResult().getData() == null) {
                            PlayGamesBridge.postError(callbackId,
                                    PlayGamesBridge.exceptionMessage(task.getException()));
                            return;
                        }
                        Snapshot snapshot = task.getResult().getData();
                        snapshot.getSnapshotContents().writeBytes(data != null ? data : new byte[0]);

                        SnapshotMetadataChange.Builder builder = new SnapshotMetadataChange.Builder();
                        if (description != null) builder.setDescription(description);
                        if (playedTimeMs >= 0)   builder.setPlayedTimeMillis(playedTimeMs);
                        if (progressValue >= 0)  builder.setProgressValue(progressValue);

                        client.commitAndClose(snapshot, builder.build())
                                .addOnCompleteListener(commit -> {
                                    if (commit.isSuccessful()) {
                                        PlayGamesBridge.postResult(callbackId, Boolean.TRUE);
                                    } else {
                                        PlayGamesBridge.postError(callbackId,
                                                PlayGamesBridge.exceptionMessage(commit.getException()));
                                    }
                                });
                    });
        } catch (Throwable t) {
            PlayGamesBridge.postError(callbackId, PlayGamesBridge.exceptionMessage(t));
        }
    }

    /** スナップショットを削除。結果は Boolean(true)。 */
    public static void delete(final long callbackId, final String uniqueName) {
        final Activity activity = PlayGamesBridge.activity();
        if (activity == null) {
            PlayGamesBridge.postError(callbackId, "PlayGamesBridge not initialized");
            return;
        }
        try {
            SnapshotsClient client = PlayGames.getSnapshotsClient(activity);
            client.open(uniqueName, false, SnapshotsClient.RESOLUTION_POLICY_MOST_RECENTLY_MODIFIED)
                    .addOnCompleteListener(open -> {
                        if (!open.isSuccessful() || open.getResult() == null
                                || open.getResult().getData() == null) {
                            PlayGamesBridge.postError(callbackId,
                                    PlayGamesBridge.exceptionMessage(open.getException()));
                            return;
                        }
                        SnapshotMetadata m = open.getResult().getData().getMetadata();
                        client.delete(m).addOnCompleteListener(del -> {
                            if (del.isSuccessful()) {
                                PlayGamesBridge.postResult(callbackId, Boolean.TRUE);
                            } else {
                                PlayGamesBridge.postError(callbackId,
                                        PlayGamesBridge.exceptionMessage(del.getException()));
                            }
                        });
                    });
        } catch (Throwable t) {
            PlayGamesBridge.postError(callbackId, PlayGamesBridge.exceptionMessage(t));
        }
    }

    /**
     * セーブ選択 UI を起動。
     * @param title UIに表示するタイトル
     * @param allowAddBtn true=新規追加ボタン表示
     * @param allowDeleteBtn true=削除ボタン表示
     * @param maxSnapshots 表示する最大件数 (0以下で無制限)
     */
    public static void showUI(final long callbackId,
                               final String title,
                               final boolean allowAddBtn,
                               final boolean allowDeleteBtn,
                               final int maxSnapshots) {
        final Activity activity = PlayGamesBridge.activity();
        if (activity == null) {
            PlayGamesBridge.postError(callbackId, "PlayGamesBridge not initialized");
            return;
        }
        try {
            SnapshotsClient client = PlayGames.getSnapshotsClient(activity);
            int max = maxSnapshots > 0 ? maxSnapshots : SnapshotsClient.DISPLAY_LIMIT_NONE;
            client.getSelectSnapshotIntent(title, allowAddBtn, allowDeleteBtn, max)
                    .addOnCompleteListener(task -> {
                        if (task.isSuccessful() && task.getResult() != null) {
                            Intent intent = task.getResult();
                            activity.runOnUiThread(() -> {
                                try {
                                    activity.startActivityForResult(intent, 0xCAFE0002);
                                    if (callbackId != 0) PlayGamesBridge.postResult(callbackId, Boolean.TRUE);
                                } catch (Throwable t) {
                                    if (callbackId != 0) PlayGamesBridge.postError(callbackId,
                                            PlayGamesBridge.exceptionMessage(t));
                                }
                            });
                        } else if (callbackId != 0) {
                            PlayGamesBridge.postError(callbackId,
                                    PlayGamesBridge.exceptionMessage(task.getException()));
                        }
                    });
        } catch (Throwable t) {
            if (callbackId != 0) PlayGamesBridge.postError(callbackId,
                    PlayGamesBridge.exceptionMessage(t));
        }
    }

    private static HashMap<String, Object> metadataToDict(SnapshotMetadata m) {
        HashMap<String, Object> d = PlayGamesNative.dict();
        if (m == null) return d;
        d.put("uniqueName", m.getUniqueName());
        d.put("description", m.getDescription());
        d.put("deviceName", m.getDeviceName());
        d.put("lastModifiedTimestamp", m.getLastModifiedTimestamp());
        d.put("playedTime", m.getPlayedTime());
        d.put("progressValue", m.getProgressValue());
        d.put("coverImageAspectRatio", (double) m.getCoverImageAspectRatio());
        return d;
    }
}
