package jp.wamsoft.krkrz.playgames;

import android.app.Activity;
import android.content.Intent;

import com.google.android.gms.games.AchievementsClient;
import com.google.android.gms.games.PlayGames;
import com.google.android.gms.games.achievement.Achievement;
import com.google.android.gms.games.achievement.AchievementBuffer;
import com.google.android.gms.common.data.DataBuffer;

/**
 * 実績 (Achievements) サブシステム。
 *
 * Play Games の実績はサーバー上に定義しておく必要があり、Java SDK 側からは
 * IDで操作する。Steam プラグインの achievements API と概念を揃え、
 * unlock / increment / load / show UI を提供。
 */
public final class PlayGamesAchievements {
    private PlayGamesAchievements() {}

    /** 実績一覧を取得。結果は Object[] of HashMap (achievementId, name, description, state, type, currentSteps, totalSteps, unlockedTimestamp)。 */
    public static void load(final long callbackId, final boolean forceReload) {
        final Activity activity = PlayGamesBridge.activity();
        if (activity == null) {
            PlayGamesBridge.postError(callbackId, "PlayGamesBridge not initialized");
            return;
        }
        try {
            AchievementsClient client = PlayGames.getAchievementsClient(activity);
            client.load(forceReload).addOnCompleteListener(task -> {
                if (!task.isSuccessful() || task.getResult() == null) {
                    PlayGamesBridge.postError(callbackId,
                            PlayGamesBridge.exceptionMessage(task.getException()));
                    return;
                }
                AchievementBuffer buffer = (AchievementBuffer) task.getResult().get();
                try {
                    if (buffer == null) {
                        PlayGamesBridge.postResult(callbackId, new Object[0]);
                        return;
                    }
                    int n = buffer.getCount();
                    Object[] arr = new Object[n];
                    for (int i = 0; i < n; ++i) {
                        Achievement a = buffer.get(i);
                        java.util.HashMap<String, Object> d = PlayGamesNative.dict();
                        d.put("achievementId", a.getAchievementId());
                        d.put("name", a.getName());
                        d.put("description", a.getDescription());
                        d.put("state", (long) a.getState());          // 0=unlocked, 1=revealed, 2=hidden
                        d.put("type", (long) a.getType());            // 0=standard, 1=incremental
                        d.put("xp", a.getXpValue());
                        if (a.getType() == Achievement.TYPE_INCREMENTAL) {
                            d.put("currentSteps", (long) a.getCurrentSteps());
                            d.put("totalSteps", (long) a.getTotalSteps());
                        }
                        if (a.getState() == Achievement.STATE_UNLOCKED) {
                            d.put("unlockedTimestamp", a.getLastUpdatedTimestamp());
                        }
                        arr[i] = d;
                    }
                    PlayGamesBridge.postResult(callbackId, arr);
                } finally {
                    if (buffer instanceof DataBuffer) {
                        try { ((DataBuffer<?>) buffer).release(); } catch (Throwable ignore) {}
                    }
                }
            });
        } catch (Throwable t) {
            PlayGamesBridge.postError(callbackId, PlayGamesBridge.exceptionMessage(t));
        }
    }

    /** 実績解除。コールバック不要なら callbackId=0 でOK。結果は Boolean(true)。 */
    public static void unlock(final long callbackId, final String achievementId) {
        final Activity activity = PlayGamesBridge.activity();
        if (activity == null) {
            PlayGamesBridge.postError(callbackId, "PlayGamesBridge not initialized");
            return;
        }
        try {
            AchievementsClient client = PlayGames.getAchievementsClient(activity);
            if (callbackId == 0) {
                client.unlock(achievementId);
            } else {
                client.unlockImmediate(achievementId).addOnCompleteListener(task -> {
                    if (task.isSuccessful()) {
                        PlayGamesBridge.postResult(callbackId, Boolean.TRUE);
                    } else {
                        PlayGamesBridge.postError(callbackId,
                                PlayGamesBridge.exceptionMessage(task.getException()));
                    }
                });
            }
        } catch (Throwable t) {
            PlayGamesBridge.postError(callbackId, PlayGamesBridge.exceptionMessage(t));
        }
    }

    /** インクリメンタル実績の進捗加算。結果は Boolean (true=これで解除された)。 */
    public static void increment(final long callbackId, final String achievementId, final int numSteps) {
        final Activity activity = PlayGamesBridge.activity();
        if (activity == null) {
            PlayGamesBridge.postError(callbackId, "PlayGamesBridge not initialized");
            return;
        }
        try {
            AchievementsClient client = PlayGames.getAchievementsClient(activity);
            if (callbackId == 0) {
                client.increment(achievementId, numSteps);
            } else {
                client.incrementImmediate(achievementId, numSteps).addOnCompleteListener(task -> {
                    if (task.isSuccessful() && task.getResult() != null) {
                        PlayGamesBridge.postResult(callbackId, task.getResult());
                    } else {
                        PlayGamesBridge.postError(callbackId,
                                PlayGamesBridge.exceptionMessage(task.getException()));
                    }
                });
            }
        } catch (Throwable t) {
            PlayGamesBridge.postError(callbackId, PlayGamesBridge.exceptionMessage(t));
        }
    }

    /** インクリメンタル実績のステップ数を絶対値で設定。 */
    public static void setSteps(final long callbackId, final String achievementId, final int numSteps) {
        final Activity activity = PlayGamesBridge.activity();
        if (activity == null) {
            PlayGamesBridge.postError(callbackId, "PlayGamesBridge not initialized");
            return;
        }
        try {
            AchievementsClient client = PlayGames.getAchievementsClient(activity);
            if (callbackId == 0) {
                client.setSteps(achievementId, numSteps);
            } else {
                client.setStepsImmediate(achievementId, numSteps).addOnCompleteListener(task -> {
                    if (task.isSuccessful() && task.getResult() != null) {
                        PlayGamesBridge.postResult(callbackId, task.getResult());
                    } else {
                        PlayGamesBridge.postError(callbackId,
                                PlayGamesBridge.exceptionMessage(task.getException()));
                    }
                });
            }
        } catch (Throwable t) {
            PlayGamesBridge.postError(callbackId, PlayGamesBridge.exceptionMessage(t));
        }
    }

    /** 実績UIを起動。結果は Boolean(true)。 */
    public static void show(final long callbackId) {
        final Activity activity = PlayGamesBridge.activity();
        if (activity == null) {
            PlayGamesBridge.postError(callbackId, "PlayGamesBridge not initialized");
            return;
        }
        try {
            AchievementsClient client = PlayGames.getAchievementsClient(activity);
            client.getAchievementsIntent().addOnCompleteListener(task -> {
                if (task.isSuccessful() && task.getResult() != null) {
                    Intent intent = task.getResult();
                    activity.runOnUiThread(() -> {
                        try {
                            activity.startActivityForResult(intent, 0xCAFE0001);
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
}
