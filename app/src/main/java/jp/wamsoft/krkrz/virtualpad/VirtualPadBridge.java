package jp.wamsoft.krkrz.virtualpad;

/**
 * 仮想パッド (SDL レイヤ実装) の Java→native ブリッジ。
 * libkrkrz.so が System.loadLibrary 済みであることが前提。
 * SDL_AttachVirtualJoystick によるゲームパッドエミュレートと post-render コールバック
 * による SDL_Renderer 直描画は native 側 (app/src/main/cpp/virtualpad) で完結している。
 * 設定 UI からはこのクラスを経由して state を切り替える。
 */
public final class VirtualPadBridge {
    private VirtualPadBridge() {}

    /** 仮想パッドの表示・入力反映を ON/OFF。 */
    public static native void setEnabled(boolean enabled);

    /** 描画の不透明度 (0.0 - 1.0)。 */
    public static native void setOpacity(float opacity);

    /** レイアウトプリセット (現状 0 のみ)。 */
    public static native void setPreset(int preset);
}
