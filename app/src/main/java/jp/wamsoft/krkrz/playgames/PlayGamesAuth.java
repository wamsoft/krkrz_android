package jp.wamsoft.krkrz.playgames;

import android.app.Activity;

import com.google.android.gms.games.GamesSignInClient;
import com.google.android.gms.games.PlayGames;
import com.google.android.gms.games.Player;
import com.google.android.gms.games.PlayersClient;

/**
 * 認証とプレイヤー情報取得。
 * Play Games Services v2 では sign-in は SDK が自動的に試みるため、
 * 通常は isAuthenticated だけ確認すれば良い。失敗時のみ signIn() で再試行する。
 */
public final class PlayGamesAuth {
    private PlayGamesAuth() {}

    /** isAuthenticated を非同期で問い合わせ、結果 (Boolean) をコールバック。 */
    public static void isAuthenticated(final long callbackId) {
        final Activity activity = PlayGamesBridge.activity();
        if (activity == null) {
            PlayGamesBridge.postError(callbackId, "PlayGamesBridge not initialized");
            return;
        }
        try {
            GamesSignInClient client = PlayGames.getGamesSignInClient(activity);
            client.isAuthenticated().addOnCompleteListener(task -> {
                if (task.isSuccessful() && task.getResult() != null) {
                    PlayGamesBridge.postResult(callbackId, task.getResult().isAuthenticated());
                } else {
                    PlayGamesBridge.postError(callbackId,
                            PlayGamesBridge.exceptionMessage(task.getException()));
                }
            });
        } catch (Throwable t) {
            PlayGamesBridge.postError(callbackId, PlayGamesBridge.exceptionMessage(t));
        }
    }

    /** インタラクティブ sign-in。結果は Boolean (true=認証済み)。 */
    public static void signIn(final long callbackId) {
        final Activity activity = PlayGamesBridge.activity();
        if (activity == null) {
            PlayGamesBridge.postError(callbackId, "PlayGamesBridge not initialized");
            return;
        }
        try {
            GamesSignInClient client = PlayGames.getGamesSignInClient(activity);
            client.signIn().addOnCompleteListener(task -> {
                if (task.isSuccessful() && task.getResult() != null) {
                    PlayGamesBridge.postResult(callbackId, task.getResult().isAuthenticated());
                } else {
                    PlayGamesBridge.postError(callbackId,
                            PlayGamesBridge.exceptionMessage(task.getException()));
                }
            });
        } catch (Throwable t) {
            PlayGamesBridge.postError(callbackId, PlayGamesBridge.exceptionMessage(t));
        }
    }

    /** 現在のプレイヤー情報を取得。結果は Map (playerId, displayName, title)。 */
    public static void currentPlayer(final long callbackId) {
        final Activity activity = PlayGamesBridge.activity();
        if (activity == null) {
            PlayGamesBridge.postError(callbackId, "PlayGamesBridge not initialized");
            return;
        }
        try {
            PlayersClient client = PlayGames.getPlayersClient(activity);
            client.getCurrentPlayer().addOnCompleteListener(task -> {
                if (task.isSuccessful() && task.getResult() != null) {
                    Player p = task.getResult();
                    java.util.HashMap<String, Object> dict = PlayGamesNative.dict();
                    dict.put("playerId", p.getPlayerId());
                    dict.put("displayName", p.getDisplayName());
                    dict.put("title", p.getTitle());
                    PlayGamesBridge.postResult(callbackId, dict);
                } else {
                    PlayGamesBridge.postError(callbackId,
                            PlayGamesBridge.exceptionMessage(task.getException()));
                }
            });
        } catch (Throwable t) {
            PlayGamesBridge.postError(callbackId, PlayGamesBridge.exceptionMessage(t));
        }
    }
}
