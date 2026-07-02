package jp.wamsoft.krkrz;

import android.app.Activity;

/**
 * Play Games 連携の facade（無効ビルド版 = no-op stub）。
 *
 * app-config.json の cmake.plugins に "playgames" が無いとき、build.gradle がこの
 * app/src/nogames/java を main の srcDir に加える (playgames 側の実装は加えない)。
 * このビルドには gms 依存も Play Games ネイティブプラグインも Manifest エントリも
 * 入らないため、ここでは何もしない。isEnabled() が false を返すことで
 * KrkrzActivity 側は drawer メニュー項目「Google Play Games」を非表示にする。
 */
public final class PlayGamesHook {
    private PlayGamesHook() {}

    public static boolean isEnabled() {
        return false;
    }

    public static void initialize(Activity activity) {
        // Play Games disabled: no-op.
    }

    public static void openAdmin(Activity activity) {
        // Play Games disabled: no-op (menu item is hidden, so this is never reached).
    }
}
