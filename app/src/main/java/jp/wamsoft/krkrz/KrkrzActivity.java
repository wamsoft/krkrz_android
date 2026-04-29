package jp.wamsoft.krkrz;

import android.content.res.AssetManager;
import android.os.Bundle;
import org.libsdl.app.SDLActivity;
import jp.wamsoft.krkrz.playgames.PlayGamesBridge;

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
        // Play Games SDK 初期化 (games_app_id が strings.xml で設定されていない場合は初期化失敗するが
        //  プラグインAPI呼び出し時にエラーが返るだけなので非Play Games利用ビルドでも安全)
        PlayGamesBridge.initialize(this);
    }

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL3", "krkrz" };
    }
}
