#!/bin/bash
#
# 「出力ファイル一式 + スクリプト」だけで任意の環境から手動インストールできる
# ポータブルインストーラを作る。配布先の前提は Java (bundletool 実行用) と adb のみ。
# このリポジトリ/フォルダ構成は不要になる。
#
#   1. AAB をビルド
#   2. bundletool build-apks --local-testing で .apks を生成 (PAD パック同梱・署名済)
#   3. .apks + bundletool.jar + install.sh/install.bat + README を 1 フォルダに同梱
#
# 使い方:
#   PROJECT_DIR=d:/work/game/xxx ./scripts/pad-package-installer.sh
#   VARIANT=Release ./scripts/pad-package-installer.sh   # 既定は Debug
#
# 出力: ${PROJECT_DIR}/build/android/pad-installer/  (この中身を丸ごと配布)
set -euo pipefail

BUILD_SYSTEM_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$BUILD_SYSTEM_DIR"

PROJECT_DIR="${PROJECT_DIR:-$BUILD_SYSTEM_DIR}"
VARIANT="${VARIANT:-Debug}"
BUNDLETOOL_VERSION="${BUNDLETOOL_VERSION:-1.17.2}"
BUNDLETOOL_JAR="$BUILD_SYSTEM_DIR/tools/bundletool-all-${BUNDLETOOL_VERSION}.jar"
OUT_DIR="$PROJECT_DIR/build/android/pad-installer"

echo "== [1/4] AAB をビルド (bundle${VARIANT}) =="
./gradlew ":app:bundle${VARIANT}"

VARIANT_DIR="${VARIANT,}"
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

echo "== [2/4] build-apks --local-testing =="
mkdir -p "$OUT_DIR"
APKS="$OUT_DIR/app.apks"
# 配布先で鍵不要にするため、split はここで debug 鍵署名して焼き込む
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

echo "== [3/4] bundletool.jar と install スクリプトを同梱 =="
cp -f "$BUNDLETOOL_JAR" "$OUT_DIR/bundletool-all-${BUNDLETOOL_VERSION}.jar"

# --- install.sh (Linux / macOS / Git Bash) ---
cat > "$OUT_DIR/install.sh" <<INSTALLSH
#!/bin/bash
# 接続中の Android 端末へインストールする。前提: Java と adb が使えること。
set -euo pipefail
DIR="\$(cd "\$(dirname "\$0")" && pwd)"
BT="\$DIR/bundletool-all-${BUNDLETOOL_VERSION}.jar"
APKS="\$DIR/app.apks"
# Git Bash (Windows) では Java に Windows パスを渡す必要がある
conv() { if command -v cygpath >/dev/null 2>&1; then cygpath -w "\$1"; else printf '%s' "\$1"; fi; }
ADB_BIN="\$(command -v adb || true)"
ARGS=()
if [ -n "\$ADB_BIN" ]; then ARGS+=(--adb="\$(conv "\$ADB_BIN")"); fi
echo "installing app.apks ..."
java -jar "\$(conv "\$BT")" install-apks --apks="\$(conv "\$APKS")" "\${ARGS[@]}"
echo "done."
INSTALLSH
chmod +x "$OUT_DIR/install.sh"

# --- install.bat (Windows cmd) ---
cat > "$OUT_DIR/install.bat" <<'INSTALLBAT'
@echo off
setlocal
set "DIR=%~dp0"
set "ADB="
for /f "delims=" %%i in ('where adb 2^>nul') do if not defined ADB set "ADB=%%i"
if defined ADB (
  java -jar "%DIR%bundletool-all-BTVER.jar" install-apks --apks="%DIR%app.apks" --adb="%ADB%"
) else (
  java -jar "%DIR%bundletool-all-BTVER.jar" install-apks --apks="%DIR%app.apks"
)
INSTALLBAT
# バージョン番号を実値へ置換 (heredoc は quoted なので後で sed)
sed -i "s/bundletool-all-BTVER.jar/bundletool-all-${BUNDLETOOL_VERSION}.jar/g" "$OUT_DIR/install.bat"

# --- README ---
cat > "$OUT_DIR/README.txt" <<README
Android PAD 手動インストーラ
============================

前提: この PC に Java (JRE 8 以上) と Android Platform Tools (adb) が入っていて、
      対象の Android 端末が USB デバッグ有効で adb devices に出ること。
      (このリポジトリやビルド環境は不要です)

インストール:
  - Linux / macOS / Git Bash :  ./install.sh
  - Windows (コマンドプロンプト) :  install.bat

初回起動時、アプリが on-demand のアセットパックをローカルストレージから展開し、
進捗表示のあとゲームが起動します (.apks に PAD パックが同梱されています)。

うまくいかないとき:
  - "adb: no devices" : 端末接続 / USB デバッグ / 認証ダイアログを確認。
  - 署名衝突で入らない : 端末に旧バージョンがある場合は先にアンインストール
      adb uninstall <パッケージ名>
  - "java: command not found" : JRE を入れて PATH を通す。

同梱物: app.apks / bundletool-all-${BUNDLETOOL_VERSION}.jar / install.sh / install.bat
README

echo "== [4/4] 完了 =="
echo "パッケージ: $OUT_DIR"
ls -lh "$OUT_DIR"
echo
echo "このフォルダを丸ごと配布すれば、Java + adb のある環境で install.sh / install.bat で入ります。"
echo "(.apks は数 GB あるので zip 圧縮効果はほぼ無し。そのまま転送推奨)"
