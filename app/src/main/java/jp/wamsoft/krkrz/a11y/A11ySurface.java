package jp.wamsoft.krkrz.a11y;

import android.content.Context;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
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
    static final int NO_FOCUS = Integer.MIN_VALUE;
    private int mA11yFocused = NO_FOCUS;   // TalkBack のフォーカスがある仮想ノード
    private View mFocusOverlay;            // それを囲む枠を描く View (A11yFocusOverlay)

    private final AccessibilityNodeProvider mProvider = new AccessibilityNodeProvider() {
        @Override
        public AccessibilityNodeInfo createAccessibilityNodeInfo(int virtualViewId) {
            AccessibilityNodeInfo n = A11yBridge.nativeCreateNodeInfo(A11ySurface.this, virtualViewId);
            // AccessKit は ACTION_CLICK を足すが setClickable はしない。押せる項目として
            // 扱わせる (TalkBack の「ダブルタップで有効化」の案内など) ために立てておく
            if (n == null) return null;
            if (n.getActionList().contains(AccessibilityNodeInfo.AccessibilityAction.ACTION_CLICK)) {
                n.setClickable(true);
            }
            adjustForNavigation(n, virtualViewId);
            return n;
        }

        @Override
        public AccessibilityNodeInfo findFocus(int focusType) {
            return A11yBridge.nativeFindFocus(A11ySurface.this, focusType);
        }

        @Override
        public boolean performAction(int virtualViewId, int action, Bundle arguments) {
            boolean r = A11yBridge.nativePerformAction(A11ySurface.this, virtualViewId, action, arguments);
            if (r && action == AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS) {
                mA11yFocused = virtualViewId;
                refreshFocusOverlay();
            } else if (r && action == AccessibilityNodeInfo.ACTION_CLEAR_ACCESSIBILITY_FOCUS
                    && mA11yFocused == virtualViewId) {
                mA11yFocused = NO_FOCUS;
                refreshFocusOverlay();
            }
            return r;
        }
    };

    public A11ySurface(Context context) {
        super(context);
        setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_YES);
        A11yBridge.setHost(this);
    }

    private final android.graphics.Rect mTmpRect = new android.graphics.Rect();
    private final int[] mTmpLoc = new int[2];

    /**
     * TalkBack のスワイプ巡回から、画面上に見えないノードを外す。
     * - 根 (この描画面): 名前があると TalkBack は根そのものを巡回の 1 項目にする (一周の
     *   折り返しで「読み上げデモ」に止まる)。名前は画面の題 (pane title) に移し、巡回では
     *   止まらせない。位置が無いと枠の計算が狂うので、描画面そのものの範囲を与える。
     * - 読み上げ用の live region (エンジンが 1 ピクセルだけ与えている): 変化は
     *   読ませたいが、巡回では止まらせない (重要でない扱い)。
     * - それ以外で大きさの無いノード (位置を持たないラベル等): 見えない扱いにする。
     */
    private void adjustForNavigation(AccessibilityNodeInfo n, int virtualViewId) {
        n.getBoundsInScreen(mTmpRect);
        if (virtualViewId == AccessibilityNodeProvider.HOST_VIEW_ID) {
            CharSequence title = n.getContentDescription();
            if (title != null && title.length() > 0) {
                n.setPaneTitle(title);
                n.setContentDescription(null);
            }
            n.setScreenReaderFocusable(false);
            if (mTmpRect.isEmpty()) {
                getLocationOnScreen(mTmpLoc);
                n.setBoundsInScreen(new android.graphics.Rect(mTmpLoc[0], mTmpLoc[1],
                        mTmpLoc[0] + getWidth(), mTmpLoc[1] + getHeight()));
            }
            return;
        }
        if (n.getLiveRegion() != View.ACCESSIBILITY_LIVE_REGION_NONE
                && mTmpRect.width() <= 1 && mTmpRect.height() <= 1) {
            n.setImportantForAccessibility(false);
            n.setFocusable(false);
            n.setScreenReaderFocusable(false);
        } else if (mTmpRect.isEmpty()) {
            n.setVisibleToUser(false);
        }
    }

    int accessibilityFocusedId() {
        return mA11yFocused;
    }

    /**
     * ツリーが変わった / フォーカスが動いたときに枠を描き直す。枠の View は
     * TalkBack が初めてフォーカスを置いたときに付ける (起動中に View を足すと
     * 描画面のレイアウトが走り、SDL のウィンドウ生成とぶつかることがあるため。
     * TalkBack を使わない人には付かない)。
     */
    void refreshFocusOverlay() {
        if (mFocusOverlay == null && mA11yFocused != NO_FOCUS
                && getParent() instanceof ViewGroup) {
            mFocusOverlay = new A11yFocusOverlay(getContext(), this);
            ((ViewGroup) getParent()).addView(mFocusOverlay, new ViewGroup.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        }
        if (mFocusOverlay != null) mFocusOverlay.invalidate();
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
