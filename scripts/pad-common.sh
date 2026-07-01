#!/bin/bash
#
# Play Asset Delivery (PAD) スクリプト共通ヘルパ。
# pad-local-install.sh / pad-fast-install.sh から source される。
#
# 提供する関数:
#   pad_resolve_adb            -> 直接実行できる adb (posix パス) を echo (adb shell/push/install 用)
#   pad_resolve_adb_win        -> bundletool --adb に渡す Windows パスの adb を echo
#   pad_pack_names <buildRoot> -> 生成済み asset pack モジュール名を1行ずつ echo
#   pad_pack_hash  <assetsDir> -> assets ツリーの内容ハッシュ (path|size|mtime の sha256)
#   pad_marker_dir <buildRoot> -> 差分判定マーカーの置き場 (未作成)
#   pad_external_lt_dir <pkg>  -> 端末側 local_testing ディレクトリの絶対パス
#
# 前提: BUILD_SYSTEM_DIR が呼び出し側で設定済み (local.properties の sdk.dir 参照に使う)。

# bundletool の --local-testing は base manifest に
#   <meta-data android:name="local_testing_dir" android:value="local_testing"/>
# を注入する。この値と一致させること (app/src/padlocal/AndroidManifest.xml も同値)。
PAD_LT_SUBDIR="local_testing"

# 端末上の local_testing ディレクトリ (bundletool install-apks / pad-fast が pack を push する場所)。
# 実体は各パックの "<pack>-master.apk"。ライブラリがここから読んで
# files/assetpacks/<pack>/<appVer>/<packVer>/assets/ へ展開する。
pad_external_lt_dir() {
  local pkg="$1"
  echo "/sdcard/Android/data/${pkg}/files/${PAD_LT_SUBDIR}"
}

# 差分判定マーカー: assetpacks/.fastpush/<name>.sha に「最後に push した内容ハッシュ」を残す。
pad_marker_dir() {
  echo "$1/assetpacks/.fastpush"
}

# 生成済み PAD asset pack モジュール名を列挙 (settings.gradle が assetpacks/<name>/ を生成する)。
# .fastpush と、assets を持たないものは除外。
pad_pack_names() {
  local br="$1"
  local d="$br/assetpacks"
  [ -d "$d" ] || return 0
  local p n
  for p in "$d"/*/; do
    [ -d "$p" ] || continue
    n="$(basename "$p")"
    [ "$n" = ".fastpush" ] && continue
    [ -d "$p/src/main/assets" ] || continue
    echo "$n"
  done
}

# assets ツリーの内容ハッシュ。runCopyRules は COPY_ATTRIBUTES で mtime を維持し、
# 変更ファイルだけ再コピーするので、データ未変更なら mtime も安定 → ハッシュも安定。
pad_pack_hash() {
  local assets="$1"
  [ -d "$assets" ] || { echo "EMPTY"; return; }
  ( cd "$assets" && find . -type f -printf '%P|%s|%T@\n' 2>/dev/null | LC_ALL=C sort | sha256sum | cut -d' ' -f1 )
}

# 直接実行用の adb (posix パス)。adb.exe を SDK から探し、無ければ PATH の adb。
pad_resolve_adb() {
  local roots=() sd r rp exe
  [ -n "${ANDROID_HOME:-}" ] && roots+=("$ANDROID_HOME")
  [ -n "${ANDROID_SDK_ROOT:-}" ] && roots+=("$ANDROID_SDK_ROOT")
  sd="$(grep -E '^sdk.dir=' "${BUILD_SYSTEM_DIR:-.}/local.properties" 2>/dev/null | head -n1 | cut -d'=' -f2- || true)"
  if [ -n "$sd" ]; then sd="${sd//\\:/:}"; sd="${sd//\\\\/\\}"; roots+=("$sd"); fi
  for r in "${roots[@]:-}"; do
    [ -z "$r" ] && continue
    rp="$(cygpath -u "$r" 2>/dev/null || echo "$r")"
    for exe in adb.exe adb; do
      [ -x "$rp/platform-tools/$exe" ] && { echo "$rp/platform-tools/$exe"; return; }
    done
  done
  command -v adb 2>/dev/null || true
}

# bundletool (Java, Windows) の --adb に渡す実ファイルの Windows パス。
pad_resolve_adb_win() {
  local roots=() sd r rp exe p
  [ -n "${ANDROID_HOME:-}" ] && roots+=("$ANDROID_HOME")
  [ -n "${ANDROID_SDK_ROOT:-}" ] && roots+=("$ANDROID_SDK_ROOT")
  sd="$(grep -E '^sdk.dir=' "${BUILD_SYSTEM_DIR:-.}/local.properties" 2>/dev/null | head -n1 | cut -d'=' -f2- || true)"
  if [ -n "$sd" ]; then sd="${sd//\\:/:}"; sd="${sd//\\\\/\\}"; roots+=("$sd"); fi
  for r in "${roots[@]:-}"; do
    [ -z "$r" ] && continue
    rp="$(cygpath -u "$r" 2>/dev/null || echo "$r")"
    for exe in adb.exe adb; do
      if [ -x "$rp/platform-tools/$exe" ]; then
        cygpath -w "$rp/platform-tools/$exe" 2>/dev/null || echo "$rp/platform-tools/$exe"
        return
      fi
    done
  done
  p="$(command -v adb 2>/dev/null || true)"
  [ -n "$p" ] && { cygpath -w "$p" 2>/dev/null || echo "$p"; }
}
