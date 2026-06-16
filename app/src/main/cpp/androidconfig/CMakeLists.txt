# androidconfig プラグイン - Java 設定 UI <-> TJS グローバル設定関数の橋渡し。
# 配置: app/src/main/cpp/androidconfig 内で C++ 完結 (playgames / virtualpad と同パターン)。
# app-config.json の cmake.plugins / cmake.staticPlugins に "androidconfig" を入れることで
# libkrkrz.so に静的組み込みされ、JNIEXPORT 関数が --whole-archive で露出される。

set(TPSTUB_DIR ${KRKRZ_BASE}/krkrz_dev/src/plugins/tp_stub CACHE PATH "Path to tp_stub directory")
include(${TPSTUB_DIR}/krkrz.cmake)

# JNI 依存のため Android 限定でビルドする (playgames / virtualpad と同じ判断)。
if(NOT ANDROID)
    message(STATUS "androidconfig plugin: skipped (non-Android target)")
    return()
endif()

krkrz_plugin(androidconfig
    NCBIND
    SOURCES
        Main.cpp
        ConfigBridge.cpp
    LIBRARIES
        android
        log
)
