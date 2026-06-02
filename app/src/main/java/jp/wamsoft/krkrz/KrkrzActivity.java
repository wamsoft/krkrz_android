package jp.wamsoft.krkrz;

import android.content.Context;
import android.content.Intent;
import android.content.res.AssetManager;
import android.os.Build;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.core.view.GravityCompat;
import androidx.drawerlayout.widget.DrawerLayout;

import com.google.android.material.navigation.NavigationView;

import org.libsdl.app.SDLActivity;
import org.libsdl.app.SDLSurface;

import jp.wamsoft.krkrz.playgames.PlayGamesBridge;
import jp.wamsoft.krkrz.ui.LicenseActivity;
import jp.wamsoft.krkrz.ui.PlayGamesAdminActivity;
import jp.wamsoft.krkrz.ui.SettingsActivity;
import jp.wamsoft.krkrz.virtualpad.VirtualPadPrefs;

/**
 * KrkrZ のメイン Activity。
 * SDLActivity を継承し、super.onCreate 後に mLayout を DrawerLayout で包み直して
 * 左端スワイプでサイドメニューを開けるようにする。
 */
public class KrkrzActivity extends SDLActivity {
    private native void setAssetManager(AssetManager assetManager);

    private DrawerLayout drawerLayout;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        installSideMenuOverlay();

        setAssetManager(getAssets());
        PlayGamesBridge.initialize(this);

        // 仮想パッドの SharedPreferences 値を native へ流す。Set* は g_state ではなく
        // モジュールレベル atomic を更新するので、virtualpad::Init() より早く呼ばれても
        // 値は保持され Init 時に反映される。
        VirtualPadPrefs.apply(this);
    }

    @Override
    protected SDLSurface createSDLSurface(Context context) {
        SDLSurface surface = super.createSDLSurface(context);
        configureSdlSurfaceZOrder(surface);
        return surface;
    }

    @Override
    protected void onResume() {
        super.onResume();
        ensureSideMenuLayer();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            ensureSideMenuLayer();
        }
    }

    @Override
    public void onBackPressed() {
        if (drawerLayout != null && drawerLayout.isDrawerOpen(GravityCompat.START)) {
            drawerLayout.closeDrawer(GravityCompat.START);
            return;
        }
        super.onBackPressed();
    }

    /**
     * SDLActivity が setContentView 済みの mLayout を取り外し、
     * DrawerLayout の中央コンテナに入れ直して再 setContentView する。
     */
    private void installSideMenuOverlay() {
        ViewGroup decorContent = findViewById(android.R.id.content);
        ViewGroup sdlParent = (ViewGroup) mLayout.getParent();
        if (sdlParent == null) return;
        sdlParent.removeView(mLayout);

        DrawerLayout drawer = (DrawerLayout) LayoutInflater.from(this)
                .inflate(R.layout.krkrz_drawer, decorContent, false);
        FrameLayout sdlContainer = drawer.findViewById(R.id.sdl_container);
        sdlContainer.addView(mLayout);
        decorContent.addView(drawer);

        NavigationView nav = drawer.findViewById(R.id.nav_view);
        configureSideMenuLayer(drawer, nav);
        nav.setNavigationItemSelectedListener(item -> {
            int id = item.getItemId();
            Intent intent = null;
            if (id == R.id.menu_settings) {
                intent = new Intent(this, SettingsActivity.class);
            } else if (id == R.id.menu_license) {
                intent = new Intent(this, LicenseActivity.class);
            } else if (id == R.id.menu_playgames) {
                intent = new Intent(this, PlayGamesAdminActivity.class);
            }
            drawer.closeDrawer(GravityCompat.START);
            if (intent != null) startActivity(intent);
            return true;
        });

        this.drawerLayout = drawer;
    }

    private void configureSdlSurfaceZOrder(SDLSurface surface) {
        if (surface == null) return;

        // SurfaceView は通常の View 階層とは別 Surface なので、明示的に背面側へ固定する。
        surface.setZOrderOnTop(false);
        surface.setZOrderMediaOverlay(false);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            surface.setElevation(0f);
            surface.setTranslationZ(0f);
        }
    }

    private void configureSideMenuLayer(DrawerLayout drawer, View drawerView) {
        drawer.setDrawerElevation(dp(16f));
        drawerView.setClickable(true);
        drawerView.setFocusable(true);

        drawer.addDrawerListener(new DrawerLayout.SimpleDrawerListener() {
            @Override
            public void onDrawerSlide(View drawerView, float slideOffset) {
                keepSideMenuAboveSurface(drawer, drawerView);
            }

            @Override
            public void onDrawerOpened(View drawerView) {
                keepSideMenuAboveSurface(drawer, drawerView);
            }
        });

        keepSideMenuAboveSurface(drawer, drawerView);
        drawer.post(() -> keepSideMenuAboveSurface(drawer, drawerView));
    }

    private void ensureSideMenuLayer() {
        if (drawerLayout == null) return;

        View drawerView = drawerLayout.findViewById(R.id.nav_view);
        if (drawerView != null) {
            drawerLayout.post(() -> keepSideMenuAboveSurface(drawerLayout, drawerView));
        }
    }

    private void keepSideMenuAboveSurface(DrawerLayout drawer, View drawerView) {
        configureSdlSurfaceZOrder(mSurface);
        drawer.bringChildToFront(drawerView);
        drawerView.bringToFront();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            drawerView.setElevation(dp(24f));
            drawerView.setTranslationZ(dp(24f));
        }
        drawer.invalidate();
    }

    private float dp(float value) {
        return value * getResources().getDisplayMetrics().density;
    }

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL3", "krkrz" };
    }
}
