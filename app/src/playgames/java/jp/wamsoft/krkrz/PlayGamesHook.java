package jp.wamsoft.krkrz;

import android.app.Activity;
import android.content.Intent;

import jp.wamsoft.krkrz.playgames.PlayGamesBridge;
import jp.wamsoft.krkrz.ui.PlayGamesAdminActivity;

/**
 * Play Games 連携の facade（有効ビルド版）。
 *
 * gms / Play Games SDK に触れる実装 (playgames パッケージ・PlayGamesAdminActivity) は
 * app/src/playgames/java 以下にあり、app-config.json の cmake.plugins に "playgames" が
 * 含まれるときだけ build.gradle が main の srcDir に加える。含まれないときは
 * app/src/nogames 側の stub 実装 (同名クラス) がコンパイルされる。
 * KrkrzActivity はこの facade だけを参照するので gms への直接依存を持たない。
 */
public final class PlayGamesHook {
    private PlayGamesHook() {}

    /** このビルドに Play Games 連携が組み込まれているか。drawer メニューの表示可否に使う。 */
    public static boolean isEnabled() {
        return true;
    }

    /** Play Games SDK 初期化。KrkrzActivity#onCreate から呼ぶ。 */
    public static void initialize(Activity activity) {
        PlayGamesBridge.initialize(activity);
    }

    /** Play Games 管理画面を開く。drawer メニュー選択時に呼ぶ。 */
    public static void openAdmin(Activity activity) {
        activity.startActivity(new Intent(activity, PlayGamesAdminActivity.class));
    }
}
