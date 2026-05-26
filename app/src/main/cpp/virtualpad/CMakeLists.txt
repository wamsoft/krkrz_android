# VirtualPad プラグイン - SDL_Renderer に半透明仮想パッドを描画 + タッチ→仮想ジョイスティック
# 配置: app/src/main/cpp/virtualpad 内で C++ も完結 (playgames と同パターン)。
# myapp.cmake (app-config.json の cmake セクションから自動生成) の TVP_PLUGINS_STATIC
# に "virtualpad" を入れることで libkrkrz.so に静的組み込みされる。

set(TPSTUB_DIR ${KRKRZ_BASE}/krkrz_dev/src/plugins/tp_stub CACHE PATH "Path to tp_stub directory")
include(${TPSTUB_DIR}/krkrz.cmake)

# 非 Android ターゲットでも描画 + 仮想ジョイスティック自体は動かせる構造だが、
# JniBridge.cpp が JNI ヘッダ依存なので Android 限定でビルドする (将来 iOS で
# 同等プラグインを作るときは JNI 部分を外して同じ VirtualPad.* を使う想定)。
if(NOT ANDROID)
    message(STATUS "virtualpad plugin: skipped (non-Android target)")
    return()
endif()

krkrz_plugin(virtualpad
    NCBIND
    SOURCES
        Main.cpp
        VirtualPad.cpp
        JniBridge.cpp
    LIBRARIES
        SDL3::SDL3
        android
        log
)

# KrkrZ エンジンが用意してる glad GLES2 ローダのヘッダパス。GL 関数はエンジン側で
# gladLoaderLoadGLES2 によって既にロード済みのグローバルポインタを共用する
# (静的プラグインなので libkrkrz.so の glad シンボルにそのまま乗る)。
target_include_directories(virtualpad PRIVATE
    ${KRKRZ_BASE}/krkrz_dev/src/core/common/glad/include)
