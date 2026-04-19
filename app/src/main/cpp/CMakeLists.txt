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

set(MYAPP_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../../myapp.cmake")
if (EXISTS "${MYAPP_CMAKE_PATH}")
	message(STATUS "Including myapp.cmake from ${MYAPP_CMAKE_PATH}")
	include(${MYAPP_CMAKE_PATH})
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
option(KRKRZ_BUILD_TESTS "build SIMD parity tests" OFF)
option(KRKRZ_USE_SJIS "use sjis encoding" YES)
add_subdirectory(${KRKRZ_DIR} ${CMAKE_CURRENT_BINARY_DIR}/krkrz)

# ----------------------------------------------------
# 追加ライブラリなどがある場合は以下に追加して下さい。
# 吉里吉里Z 本体のターゲット名は "krrkz" です
# ----------------------------------------------------
