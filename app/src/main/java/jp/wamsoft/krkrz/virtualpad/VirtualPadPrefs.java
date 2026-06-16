package jp.wamsoft.krkrz.virtualpad;

import android.content.Context;
import android.content.SharedPreferences;

/**
 * 仮想パッドの設定値を SharedPreferences に保持するヘルパ。
 * 変更時は同時に native (VirtualPadBridge) へも反映するので、UI から呼ぶだけで
 * 永続化と即時反映が両立する。アプリ起動時は {@link #apply(Context)} で保存値を
 * native へ流し込む。
 */
public final class VirtualPadPrefs {
    private VirtualPadPrefs() {}

    private static final String NAME              = "virtualpad";
    private static final String K_ENABLED         = "enabled";
    private static final String K_TRANSLUCENT     = "translucent";

    public static final boolean DEFAULT_ENABLED     = false;
    public static final boolean DEFAULT_TRANSLUCENT = true;

    private static final float OPACITY_TRANSLUCENT = 0.55f;
    private static final float OPACITY_OPAQUE      = 1.0f;

    public static SharedPreferences prefs(Context ctx) {
        return ctx.getSharedPreferences(NAME, Context.MODE_PRIVATE);
    }

    public static boolean isEnabled(Context ctx) {
        return prefs(ctx).getBoolean(K_ENABLED, DEFAULT_ENABLED);
    }

    public static void setEnabled(Context ctx, boolean enabled) {
        prefs(ctx).edit().putBoolean(K_ENABLED, enabled).apply();
        VirtualPadBridge.setEnabled(enabled);
    }

    public static boolean isTranslucent(Context ctx) {
        return prefs(ctx).getBoolean(K_TRANSLUCENT, DEFAULT_TRANSLUCENT);
    }

    public static void setTranslucent(Context ctx, boolean translucent) {
        prefs(ctx).edit().putBoolean(K_TRANSLUCENT, translucent).apply();
        VirtualPadBridge.setOpacity(translucent ? OPACITY_TRANSLUCENT : OPACITY_OPAQUE);
    }

    /** アプリ起動時 (KrkrzActivity.onCreate 内) に保存値を native 側へ反映する。 */
    public static void apply(Context ctx) {
        VirtualPadBridge.setEnabled(isEnabled(ctx));
        VirtualPadBridge.setOpacity(isTranslucent(ctx) ? OPACITY_TRANSLUCENT : OPACITY_OPAQUE);
    }
}
