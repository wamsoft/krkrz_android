package jp.wamsoft.krkrz.a11y;

import android.content.Context;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeProvider;

import org.libsdl.app.SDLSurface;

/**
 * 読み上げ (TalkBack) に対応した SDL の描画面。
 *
 * エンジンが描いた UI (Elements のダイアログ、setGameA11y / a11yLayers のノード) を
 * 仮想ノードとして見せる。ノードの中身・操作・タッチ探索 (hover) は
 * AccessKit (エンジン側) が答える。
 */
public class A11ySurface extends SDLSurface {
    private final AccessibilityNodeProvider mProvider = new AccessibilityNodeProvider() {
        @Override
        public AccessibilityNodeInfo createAccessibilityNodeInfo(int virtualViewId) {
            AccessibilityNodeInfo n = A11yBridge.nativeCreateNodeInfo(A11ySurface.this, virtualViewId);
            // AccessKit は ACTION_CLICK を足すが setClickable はしない。押せる項目として
            // 扱わせる (TalkBack の「ダブルタップで有効化」の案内など) ために立てておく
            if (n != null && n.getActionList().contains(AccessibilityNodeInfo.AccessibilityAction.ACTION_CLICK)) {
                n.setClickable(true);
            }
            return n;
        }

        @Override
        public AccessibilityNodeInfo findFocus(int focusType) {
            return A11yBridge.nativeFindFocus(A11ySurface.this, focusType);
        }

        @Override
        public boolean performAction(int virtualViewId, int action, Bundle arguments) {
            return A11yBridge.nativePerformAction(A11ySurface.this, virtualViewId, action, arguments);
        }
    };

    public A11ySurface(Context context) {
        super(context);
        setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_YES);
        A11yBridge.setHost(this);
    }

    @Override
    public AccessibilityNodeProvider getAccessibilityNodeProvider() {
        return mProvider;
    }

    @Override
    public boolean onHoverEvent(MotionEvent event) {
        // タッチ探索 (TalkBack 中のなぞり)。読み上げが動いていなければ native は false
        if (A11yBridge.nativeOnHover(this, event.getAction(), event.getX(), event.getY())) {
            return true;
        }
        return super.onHoverEvent(event);
    }
}
