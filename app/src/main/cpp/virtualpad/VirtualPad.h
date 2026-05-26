#pragma once

// VirtualPad プラグイン: SDL_Renderer 上に半透明の仮想ゲームパッドを描画し、
// タッチ入力を SDL_AttachVirtualJoystick 経由でゲームパッド入力としてエンジンに渡す。
// KrkrZ エンジン (sdl3/visual/PostRenderCallback.h) の post-render フックを利用するため、
// SDL3 + KrkrZ のレンダリングパイプライン上で動作する (Android 専用ではない実装)。

namespace virtualpad {

// プラグイン PreRegist / PostRegist から呼ばれる。冪等。
void Init();
void Shutdown();

// 表示・入力の状態制御 (Java 側 SharedPreferences と連動)
void SetEnabled(bool enabled);
void SetOpacity(float opacity);  // 0.0 - 1.0
void SetPreset(int preset);      // 0 = bottom (現状唯一のプリセット)

} // namespace virtualpad
