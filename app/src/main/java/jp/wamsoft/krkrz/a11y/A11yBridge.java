package jp.wamsoft.krkrz.a11y;

import android.content.Context;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.accessibility.AccessibilityManager;
import android.view.accessibility.AccessibilityNodeInfo;

import org.libsdl.app.SDLSurface;

/**
 * 読み上げ (TalkBack) の Java 側の窓口。
 *
 * Android のアクセシビリティは UI スレッドで View に問い合わせる。SDL の描画面
 * ({@link A11ySurface}) がそれを受け、native (a11y プラグイン → エンジンの
 * TVPA11yAndroid*) へ渡す。エンジンは別スレッド (SDL のスレッド) で動くので、
 * ツリーが変わったら {@link #requestUpdate()} が UI スレッドへ更新を回す。
 *
 * a11y プラグインを組み込んでいないビルドでは native が無いので、
 * {@link #init} が false になり、通常の SDLSurface のまま動く。
 */
public final class A11yBridge {
    private static final String TAG = "A11yBridge";

    private static boolean sAvailable;
    private static volatile View sHost;   // いまの描画面 (Activity の作り直しで替わる)

    private A11yBridge() {}

    /** Activity.onCreate (super.onCreate より前) で呼ぶ。読み上げが使えるビルドなら true。 */
    public static boolean init(Context context) {
        if (sAvailable) return true;
        try {
            nativeRegister();
        } catch (UnsatisfiedLinkError e) {
            Log.i(TAG, "a11y plugin not built in; screen reader support disabled");
            return false;
        }
        sAvailable = true;
        AccessibilityManager am =
                (AccessibilityManager) context.getApplicationContext()
                        .getSystemService(Context.ACCESSIBILITY_SERVICE);
        if (am != null) {
            // TalkBack を切ったことは AccessKit 側からは分からないので、ここで伝える
            am.addAccessibilityStateChangeListener(enabled -> nativeSetEnabled(enabled));
        }
        return true;
    }

    public static boolean isAvailable() {
        return sAvailable;
    }

    /** 描画面を作る (使えないビルドでは null を返すので、呼び出し側が既定のものを作る)。 */
    public static SDLSurface createSurface(Context context) {
        return sAvailable ? new A11ySurface(context) : null;
    }

    /**
     * 描画面の上に TalkBack のフォーカス枠を描く View を重ねる (Activity.onCreate の
     * super.onCreate 後、描画面が親に入ってから呼ぶ)。読み上げ対応の描画面でなければ何もしない。
     */
    public static void attachFocusOverlay(View surface) {
        if (!(surface instanceof A11ySurface)) return;
        android.view.ViewParent p = surface.getParent();
        if (!(p instanceof android.view.ViewGroup)) return;
        A11ySurface s = (A11ySurface) surface;
        A11yFocusOverlay overlay = new A11yFocusOverlay(surface.getContext(), s);
        ((android.view.ViewGroup) p).addView(overlay, new android.view.ViewGroup.LayoutParams(
                android.view.ViewGroup.LayoutParams.MATCH_PARENT,
                android.view.ViewGroup.LayoutParams.MATCH_PARENT));
        s.setFocusOverlay(overlay);
    }

    static void setHost(View host) {
        sHost = host;
    }

    /** native (SDL のスレッド) から呼ばれる: UI スレッドでツリーの更新を流す。 */
    @SuppressWarnings("unused")
    static void requestUpdate() {
        View host = sHost;
        if (host != null) host.post(() -> {
            nativeUpdate(host);
            if (host instanceof A11ySurface) ((A11ySurface) host).refreshFocusOverlay();
        });
    }

    static native void nativeRegister();
    static native AccessibilityNodeInfo nativeCreateNodeInfo(View host, int virtualViewId);
    static native AccessibilityNodeInfo nativeFindFocus(View host, int focusType);
    static native boolean nativePerformAction(View host, int virtualViewId, int action, Bundle arguments);
    static native boolean nativeOnHover(View host, int action, float x, float y);
    static native void nativeUpdate(View host);
    static native void nativeSetEnabled(boolean enabled);
}
