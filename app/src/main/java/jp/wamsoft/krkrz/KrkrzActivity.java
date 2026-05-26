package jp.wamsoft.krkrz;

import android.content.Intent;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.core.view.GravityCompat;
import androidx.drawerlayout.widget.DrawerLayout;

import com.google.android.material.navigation.NavigationView;

import org.libsdl.app.SDLActivity;

import jp.wamsoft.krkrz.playgames.PlayGamesBridge;
import jp.wamsoft.krkrz.ui.LicenseActivity;
import jp.wamsoft.krkrz.ui.PlayGamesAdminActivity;
import jp.wamsoft.krkrz.ui.SettingsActivity;

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

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL3", "krkrz" };
    }
}
