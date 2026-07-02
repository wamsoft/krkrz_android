package jp.wamsoft.krkrz.playgames;

import android.app.Activity;
import android.util.Log;

import com.google.android.gms.games.PlayGamesSdk;

/**
 * Play Games サブシステム共通の Activity 保持と SDK 初期化を担当。
 *
 * KrkrzActivity#onCreate の冒頭から initialize(this) を呼ぶ前提。
 * Native から TJS 経由で来る Play Games 系呼び出しはここで保持する Activity を
 * 使って AchievementsClient / SnapshotsClient / GamesSignInClient を取得する。
 */
public final class PlayGamesBridge {
    private static final String TAG = "PlayGamesBridge";

    private static volatile Activity sActivity;
    private static volatile boolean sInitialized = false;

    private PlayGamesBridge() {}

    public static synchronized void initialize(Activity activity) {
        if (sInitialized && sActivity == activity) return;
        sActivity = activity;
        try {
            PlayGamesSdk.initialize(activity.getApplicationContext());
            sInitialized = true;
            Log.i(TAG, "PlayGamesSdk initialized");
        } catch (Throwable t) {
            Log.e(TAG, "PlayGamesSdk.initialize failed", t);
            sInitialized = false;
        }
    }

    public static Activity activity() {
        return sActivity;
    }

    public static boolean isInitialized() {
        return sInitialized;
    }

    static void postError(long callbackId, String message) {
        if (callbackId == 0) return;
        PlayGamesNative.nativePostError(callbackId, message != null ? message : "(null)");
    }

    static void postResult(long callbackId, Object result) {
        if (callbackId == 0) return;
        PlayGamesNative.nativePostResult(callbackId, result);
    }

    static String exceptionMessage(Throwable t) {
        if (t == null) return "unknown error";
        String m = t.getMessage();
        return (m != null) ? (t.getClass().getSimpleName() + ": " + m) : t.getClass().getSimpleName();
    }
}
