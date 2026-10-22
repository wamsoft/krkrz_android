# a11y プラグイン - 読み上げ (TalkBack) の JNI 受け口
# Java の jp.wamsoft.krkrz.a11y.A11yBridge / A11ySurface から呼ばれ、エンジン本体の
# TVPA11yAndroid* (krkrz_dev src/core/sdl3/environ/A11yAndroid.cpp) へ転送する。
# app-config.json の cmake.plugins / staticPlugins に "a11y" を入れると、
# app/build.gradle が -DKRKRZ_USE_A11Y=ON も渡す (docs/android-accessibility.md)。

set(TPSTUB_DIR ${KRKRZ_BASE}/krkrz_dev/src/plugins/tp_stub CACHE PATH "Path to tp_stub directory")
include(${TPSTUB_DIR}/krkrz.cmake)

if(NOT ANDROID)
    message(STATUS "a11y plugin: skipped (non-Android target)")
    return()
endif()
if(NOT KRKRZ_USE_A11Y)
    # エンジン側の口 (TVPA11yAndroid*) が無いのでリンクできない。Java 側は
    # native が無ければ読み上げを使わずに動く。
    message(STATUS "a11y plugin: skipped (KRKRZ_USE_A11Y=OFF)")
    return()
endif()

krkrz_plugin(a11y
    SOURCES
        JniBridge.cpp
    LIBRARIES
        android
)
