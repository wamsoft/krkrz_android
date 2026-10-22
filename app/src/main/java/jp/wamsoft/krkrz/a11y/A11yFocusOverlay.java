package jp.wamsoft.krkrz.a11y;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.os.Build;
import android.view.View;
import android.view.accessibility.AccessibilityManager;
import android.view.accessibility.AccessibilityNodeInfo;

/**
 * TalkBack のフォーカス枠を描く透明な View (描画面の上に重ねる)。
 *
 * 枠は本来ウィンドウ (ViewRootImpl) が描くが、SDL の描画面はウィンドウの
 * 背面にある SurfaceView で、その範囲はウィンドウ側では「透明」扱いになる。
 * View ではない枠はそこに描かれても表示されないので、ここで描く。
 */
public class A11yFocusOverlay extends View {
    private final A11ySurface mSurface;
    private final Paint mPaint = new Paint();
    private final Rect mRect = new Rect();
    private final int[] mLoc = new int[2];

    public A11yFocusOverlay(Context context, A11ySurface surface) {
        super(context);
        mSurface = surface;
        setWillNotDraw(false);
        setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        setClickable(false);
        setFocusable(false);
        mPaint.setStyle(Paint.Style.STROKE);
        AccessibilityManager am =
                (AccessibilityManager) context.getSystemService(Context.ACCESSIBILITY_SERVICE);
        int color = 0xFF4CAF50;
        int width = (int) (4 * context.getResources().getDisplayMetrics().density);
        if (am != null && Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            color = am.getAccessibilityFocusColor();
            width = am.getAccessibilityFocusStrokeWidth();
        }
        mPaint.setColor(color);
        mPaint.setStrokeWidth(width);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        int id = mSurface.accessibilityFocusedId();
        if (id == A11ySurface.NO_FOCUS) return;
        AccessibilityNodeInfo n = mSurface.getAccessibilityNodeProvider().createAccessibilityNodeInfo(id);
        if (n == null) return;
        n.getBoundsInScreen(mRect);
        getLocationOnScreen(mLoc);
        mRect.offset(-mLoc[0], -mLoc[1]);
        float half = mPaint.getStrokeWidth() / 2;
        canvas.drawRect(mRect.left + half, mRect.top + half, mRect.right - half, mRect.bottom - half, mPaint);
    }
}
