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
    private static final String K_OPACITY         = "opacity_pct";
    // 旧設定 (半透明 ON/OFF ブール)。opacity_pct 未保存時のみ移行に使う。
    private static final String K_TRANSLUCENT     = "translucent";

    public static final boolean DEFAULT_ENABLED     = false;

    // 不透明度はパーセント (スライダー値)。完全に消えて操作不能になるのを防ぐため
    // 下限を設ける。
    public static final int OPACITY_MIN     = 10;
    public static final int OPACITY_MAX     = 100;
    public static final int OPACITY_DEFAULT = 55;   // 旧「半透明」相当
    private static final int OPACITY_LEGACY_OPAQUE = 100;  // 旧「不透明」相当

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

    /** 不透明度 (パーセント {@link #OPACITY_MIN}〜{@link #OPACITY_MAX})。 */
    public static int getOpacityPercent(Context ctx) {
        SharedPreferences p = prefs(ctx);
        int pct;
        if (p.contains(K_OPACITY)) {
            pct = p.getInt(K_OPACITY, OPACITY_DEFAULT);
        } else if (p.contains(K_TRANSLUCENT)) {
            // 旧ブール設定からの移行: ON=半透明(既定) / OFF=不透明。
            pct = p.getBoolean(K_TRANSLUCENT, true) ? OPACITY_DEFAULT : OPACITY_LEGACY_OPAQUE;
        } else {
            pct = OPACITY_DEFAULT;
        }
        return clamp(pct);
    }

    public static void setOpacityPercent(Context ctx, int pct) {
        pct = clamp(pct);
        prefs(ctx).edit().putInt(K_OPACITY, pct).apply();
        VirtualPadBridge.setOpacity(pct / 100.0f);
    }

    private static int clamp(int pct) {
        return pct < OPACITY_MIN ? OPACITY_MIN : (pct > OPACITY_MAX ? OPACITY_MAX : pct);
    }

    /** アプリ起動時 (KrkrzActivity.onCreate 内) に保存値を native 側へ反映する。 */
    public static void apply(Context ctx) {
        VirtualPadBridge.setEnabled(isEnabled(ctx));
        VirtualPadBridge.setOpacity(getOpacityPercent(ctx) / 100.0f);
    }
}
