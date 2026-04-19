package jp.wamsoft.krkrz;

import android.content.res.AssetManager;
import android.os.Bundle;
import org.libsdl.app.SDLActivity;

/**
 * KrkrZのメインActivityクラス
 * SDLActivityを継承している
 */
public class KrkrzActivity extends SDLActivity {
    // JNI 側 (andres.cpp) の Java_jp_wamsoft_krkrz_KrkrzActivity_setAssetManager と一致する宣言
    private native void setAssetManager(AssetManager assetManager);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // アセットマネージャーをネイティブ層へ渡す
        setAssetManager(getAssets());
    }

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL3", "krkrz" };
    }
}
