# license プラグイン - KrkrZ エンジン内部の権利情報文字列を Java へ橋渡し。
# TJS バインディングは不要 (純粋に JNI 関数を libkrkrz.so に露出させたいだけ)
# なので NCBIND は付けず、krkrz_plugin の static + --whole-archive 機構だけ利用する。

set(TPSTUB_DIR ${KRKRZ_BASE}/krkrz_dev/src/plugins/tp_stub CACHE PATH "Path to tp_stub directory")
include(${TPSTUB_DIR}/krkrz.cmake)

if(NOT ANDROID)
    message(STATUS "license plugin: skipped (non-Android target)")
    return()
endif()

krkrz_plugin(license
    SOURCES
        JniBridge.cpp
    LIBRARIES
        android
        log
)
