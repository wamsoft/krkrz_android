cmake_minimum_required(VERSION 3.20)

# Set vcpkg triplet based on ANDROID_ABI
# 本来は vcpkg 側で自動判定されるべきだがうまくされないので手動設定
# これをしないと vcpkg.json で x64-android 用のライブラリが適切にインストールされない
if(DEFINED ANDROID_ABI)
	if(ANDROID_ABI STREQUAL "armeabi-v7a")
		set(VCPKG_TARGET_TRIPLET "arm-android")
	elseif(ANDROID_ABI STREQUAL "arm64-v8a")
		set(VCPKG_TARGET_TRIPLET "arm64-android")
	elseif(ANDROID_ABI STREQUAL "x86")
		set(VCPKG_TARGET_TRIPLET "x86-android")
	elseif(ANDROID_ABI STREQUAL "x86_64")
		set(VCPKG_TARGET_TRIPLET "x64-android")
	else()
		message(WARNING "Unknown ANDROID_ABI '${ANDROID_ABI}'. VCPKG_TARGET_TRIPLET not changed.")
	endif()
	message(STATUS "VCPKG_TARGET_TRIPLET set to '${VCPKG_TARGET_TRIPLET}' for ANDROID_ABI='${ANDROID_ABI}'")
else()
	message(WARNING "ANDROID_ABI not defined; VCPKG_TARGET_TRIPLET not changed.")
endif()

# 引数で渡された ANDROID_NDK_HOME を環境変数に設定する
# ※ vcpkg の動作時に必要になるようなので android 側が期待してる値とそろえる想定
if (DEFINED ANDROID_NDK_HOME)
    set(ENV{ANDROID_NDK_HOME} "${ANDROID_NDK_HOME}")
endif()

project(krkrz_android)

# 環境変数KRKRZ_BASE に krkrz用の各種ソースのベースパスを設定
cmake_path(SET KRKRZ_BASE "$ENV{KRKRZ_BASE}")

# myapp.cmake は build.gradle の generateMyappCmake タスクが
# ${PROJECT_DIR}/build/android/app/generated/cmake/myapp.cmake へ生成し、
# build.gradle が defaultConfig.externalNativeBuild.cmake.arguments で
# -DMYAPP_CMAKE_FILE=<absolute path> として渡してくる。
if (DEFINED MYAPP_CMAKE_FILE AND EXISTS "${MYAPP_CMAKE_FILE}")
	message(STATUS "Including myapp.cmake from ${MYAPP_CMAKE_FILE}")
	include("${MYAPP_CMAKE_FILE}")
endif()

# ----------------------------------------------------
# krkrz setup
# ----------------------------------------------------

set(KRKRZ_DEFINES_PLATFORM
# ファイル名をノーマライズしない
TVP_NO_NORMALIZE_PATH
# ファイル検索時はケース無視
TVP_AUTOPATH_IGNORECASE
# ローカルパス参照のcaseを強制参照
TVP_LOCALFILE_FORCE_CASESENSITIVE
# PROVINCE/MASK ロード除外
TVP_DONT_AUTOLOAD_PROVINCE
TVP_DONT_AUTOLOAD_MASK
# ファイル保存時にストレージ系のキャッシュをクリアしない
TVP_DONT_CLEAR_AUTOPATH_CACHE
# miniaudio のランタイムを動的処理しない
MA_NO_RUNTIME_LINKING
)

# ----------------------------------------------------
# prefab 配置されてる SDLを使用する指定
# ----------------------------------------------------

find_package(SDL3 REQUIRED CONFIG)

# ----------------------------------------------------
# 組み込みプラグイン定義
# ----------------------------------------------------

# 共通プラグインフォルダ
set(KRKRZ_DIR ${KRKRZ_BASE}/krkrz_dev/src/core)
set(PLUGINS_DIR ${KRKRZ_BASE}/krkrz_dev/src/plugins)

# プラグイン参照先
LIST(APPEND TVP_PLUGIN_FOLDERS
   ${PLUGINS_DIR}
)

# プラグイン用基本フォルダ
set(TPSTUB_DIR ${KRKRZ_DIR}/tp_stub)
set(NCBIND_DIR ${PLUGINS_DIR}/ncbind)
set(SIMPLEBIND_DIR ${PLUGINS_DIR}/simplebinder)

## 全て組み込みプラグインにする
#LIST(APPEND TVP_PLUGINS_STATIC
#  ${TVP_PLUGINS}
#)

# 共通設定
option(KRKRZ_ENABLE_DAP "enable VSCode Debug Adapter Protocol server" OFF)
option(KRKRZ_USE_LOGCORE "enable LogCore" OFF)
option(KRKRZ_REPL "enable interactive TJS REPL" OFF)
option(KRKRZ_BUILD_TESTS "build SIMD parity tests" OFF)
# Elements (汎用ダイアログ機構 / cycfi::elements + thorvg + elements_modal)
# Android でも有効化する (libkrkrz.so に thorvg/elements/harfbuzz を組み込む)。
# 注意: 有効時は thorvg のサンプル exe (tvg-ft-text-sample 等) も add_executable
# されるが、これらは Android NDK/bionic で通らない (basename 衝突) 上に不要なので、
# app/build.gradle の externalNativeBuild で targets 'krkrz' に絞ってビルド対象から
# 除外している (libkrkrz.so とその依存のみビルド)。
option(KRKRZ_USE_ELEMENTS "embed Elements dialog UI" ON)

# REPL (adb 経由の TJS 対話シェル) は Debug ビルドのみ有効化する。
# - icline (行編集) は NDK 未対応。Android は console を使わず socket 経路のみ
#   なので KRKRZ_REPL_LINE_EDIT=OFF で icline の FetchContent を回避する
#   (REPL.cpp は KRKRZ_REPL_NO_ICLINE で fgets fallback になるが、Android では
#    console スレッド自体を起動しないので実際には使われない)。
# - 実際に listen するのは KrkrzActivity が debuggable 時に env KRKRZ_REPL_SOCKET
#   を設定したときだけ (tTVPReplSocketChannel::ShouldStart)。Release には REPL 自体
#   がコンパイルされない。
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(KRKRZ_REPL ON CACHE BOOL "" FORCE)
    set(KRKRZ_REPL_LINE_EDIT OFF CACHE BOOL "" FORCE)
    message(STATUS "[krkrz] REPL enabled (Debug): socket channel via KRKRZ_REPL_SOCKET")
endif()

add_subdirectory(${KRKRZ_DIR} ${CMAKE_CURRENT_BINARY_DIR}/krkrz)

# ----------------------------------------------------
# 追加ライブラリなどがある場合は以下に追加して下さい。
# 吉里吉里Z 本体のターゲット名は "krrkz" です
# ----------------------------------------------------
