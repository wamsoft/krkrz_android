#!/bin/bash
#
# Play Asset Delivery (PAD) をローカル (Play を通さず) 実機で検証するスクリプト。
#
#   1. AAB (base + 全 asset pack) をビルド
#   2. bundletool build-apks --local-testing で APK セットを生成
#      (--local-testing により端末上で AssetPackManager がローカル配置のパックを解決し、
#       本番と同じランタイム経路がオフラインで動く)
#   3. bundletool install-apks で接続中の端末へインストール
#
# 使い方:
#   PROJECT_DIR=d:/work/game/xxx ./scripts/pad-local-install.sh
#   VARIANT=Release ./scripts/pad-local-install.sh   # 既定は Debug
#
# 依存: Android SDK (adb) / JDK (java) / curl。bundletool は自動ダウンロードする。
set -euo pipefail

BUILD_SYSTEM_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$BUILD_SYSTEM_DIR"
# shellcheck source=scripts/pad-common.sh
source "$BUILD_SYSTEM_DIR/scripts/pad-common.sh"

PROJECT_DIR="${PROJECT_DIR:-$BUILD_SYSTEM_DIR}"
VARIANT="${VARIANT:-Debug}"
BUNDLETOOL_VERSION="${BUNDLETOOL_VERSION:-1.17.2}"
BUNDLETOOL_JAR="$BUILD_SYSTEM_DIR/tools/bundletool-all-${BUNDLETOOL_VERSION}.jar"

# ---- adb 検出 ----
# bundletool は Java (Windows) なので --adb には**実ファイルの Windows パス**が要る
# (PATH 上の "adb" 文字列や git-bash の posix パスでは checkFileExistsAndExecutable に失敗)。
# ANDROID_HOME / ANDROID_SDK_ROOT / local.properties(sdk.dir) から adb.exe を探し、
# cygpath -w で Windows パスに変換して返す。
resolve_adb_win() {
  local roots=()
  [ -n "${ANDROID_HOME:-}" ] && roots+=("$ANDROID_HOME")
  [ -n "${ANDROID_SDK_ROOT:-}" ] && roots+=("$ANDROID_SDK_ROOT")
  local sd
  sd="$(grep -E '^sdk.dir=' "$BUILD_SYSTEM_DIR/local.properties" 2>/dev/null | head -n1 | cut -d'=' -f2- || true)"
  if [ -n "$sd" ]; then sd="${sd//\\:/:}"; sd="${sd//\\\\/\\}"; roots+=("$sd"); fi
  if [ ${#roots[@]} -gt 0 ]; then
    local r rp exe
    for r in "${roots[@]}"; do
      rp="$(cygpath -u "$r" 2>/dev/null || echo "$r")"
      for exe in adb.exe adb; do
        if [ -x "$rp/platform-tools/$exe" ]; then
          cygpath -w "$rp/platform-tools/$exe" 2>/dev/null || echo "$rp/platform-tools/$exe"
          return
        fi
      done
    done
  fi
  local p
  p="$(command -v adb 2>/dev/null || true)"
  [ -n "$p" ] && { cygpath -w "$p" 2>/dev/null || echo "$p"; }
}
ADB_WIN="$(resolve_adb_win)"
if [ -z "${ADB_WIN:-}" ]; then
  echo "ERROR: adb が見つかりません。ANDROID_HOME か local.properties の sdk.dir を設定してください。"
  exit 1
fi
echo "adb: $ADB_WIN"

echo "== [1/3] AAB をビルド (bundle${VARIANT}) =="
./gradlew ":app:bundle${VARIANT}"

# 出力は variant 別サブフォルダ (bundle/<variant 先頭小文字>/)。variant フォルダに限定して探す
# (bundle/ 全体だと別 variant の古い AAB を拾う)。
VARIANT_DIR="${VARIANT,}"   # Release -> release, Debug -> debug
BUNDLE_DIR="$PROJECT_DIR/build/android/app/outputs/bundle/$VARIANT_DIR"
AAB="$(find "$BUNDLE_DIR" -name '*.aab' 2>/dev/null | head -n1 || true)"
if [ -z "${AAB:-}" ]; then
  echo "ERROR: AAB が見つかりません ($BUNDLE_DIR)"; exit 1
fi
echo "AAB: $AAB"

# ---- bundletool を用意 ----
if [ ! -f "$BUNDLETOOL_JAR" ]; then
  echo "== bundletool ${BUNDLETOOL_VERSION} をダウンロード =="
  mkdir -p "$BUILD_SYSTEM_DIR/tools"
  curl -L -o "$BUNDLETOOL_JAR" \
    "https://github.com/google/bundletool/releases/download/${BUNDLETOOL_VERSION}/bundletool-all-${BUNDLETOOL_VERSION}.jar"
fi

APKS="$PROJECT_DIR/build/android/app/outputs/bundle/local-testing.apks"

echo "== [2/3] build-apks --local-testing =="
# デバッグ用途なので debug keystore で署名 (無ければ bundletool が既定 debug 鍵を使う)
DEBUG_KS="$HOME/.android/debug.keystore"
SIGN_ARGS=()
if [ -f "$DEBUG_KS" ]; then
  SIGN_ARGS=(--ks="$DEBUG_KS" --ks-pass=pass:android --ks-key-alias=androiddebugkey --key-pass=pass:android)
fi
rm -f "$APKS"
java -jar "$BUNDLETOOL_JAR" build-apks \
  --bundle="$AAB" \
  --output="$APKS" \
  --local-testing \
  "${SIGN_ARGS[@]}"

echo "== [3/3] install-apks (接続端末へ) =="
java -jar "$BUNDLETOOL_JAR" install-apks --apks="$APKS" --adb="$ADB_WIN"

# 差分マーカーを更新: いま端末へ入れた各 pack の内容ハッシュを記録しておく。
# これで以後 make pad-fast が「変更なし」を判定でき、pack の再 push を省ける。
MARK="$(pad_marker_dir "$PROJECT_DIR/build/android")"
mkdir -p "$MARK"
while IFS= read -r name; do
  [ -z "$name" ] && continue
  pad_pack_hash "$PROJECT_DIR/build/android/assetpacks/$name/src/main/assets" > "$MARK/$name.sha"
done < <(pad_pack_names "$PROJECT_DIR/build/android")

echo
echo "完了。初回起動で BootstrapActivity が各 on-demand パックを fetch → 進捗表示 → 起動します。"
echo "以後コードだけ直す場合は make pad-fast が速い (pack 未変更なら再 push しません)。"
echo "ログ確認: adb logcat -s BootstrapActivity:* PadAssetResolver:*"
