# PlayGames プラグイン - Google Play Games Services への TJS バインディング
# 配置 (b): app/src/main/cpp/playgames 内で C++ も完結。
# 必ず static plugin として libkrkrz.so に組み込まれる前提 (myapp.cmake で
# TVP_PLUGINS_STATIC に追加)。

set(TPSTUB_DIR ${KRKRZ_BASE}/krkrz_dev/src/plugins/tp_stub CACHE PATH "Path to tp_stub directory")
include(${TPSTUB_DIR}/krkrz.cmake)

# Android 以外のターゲットでは何も生成しない (誤って組み込まれても無害)
if(NOT ANDROID)
    message(STATUS "playgames plugin: skipped (non-Android target)")
    return()
endif()

krkrz_plugin(playgames
    NCBIND
    SOURCES
        Main.cpp
        JniBridge.cpp
        PlayGamesAuth.cpp
        PlayGamesAchievements.cpp
        PlayGamesSnapshots.cpp
    LIBRARIES
        log
        android
)
