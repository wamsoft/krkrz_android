#!/bin/bash
#
# PAD 軽量イテレーション用インストール。
#
# 通常の make pad-install は AAB→bundletool で **全 pack (数 GB) を毎回端末へ push** するため、
# コードだけ直したいときに重い。このスクリプトは:
#
#   1. 素の assembleDebug (-PpadLocalTesting) をビルド → base APK に local_testing_dir 配線を付与
#   2. その 1 APK だけを adb install -r (数十 MB・数秒)
#   3. 各 pack の内容ハッシュを前回 push 時のマーカーと比較し、**変わった pack だけ** を
#      bundletool で作り直して push。push した pack は端末の展開先を消して再展開を強制する
#   4. 変わってなければ pack には一切触らず、端末に既にある展開済みデータをそのまま再利用
#   5. アプリを起動
#
# → データ未変更なら「コードのビルド + APK 1個 install」だけで済む。
#
# 前提: 一度 make pad-install を実行して pack が端末に配置済みであること
#       (このスクリプトはマーカーを基準に差分だけ反映する。初回や pack 名変更時は全 push になる)。
#
# 使い方:
#   PROJECT_DIR=d:/work/game/xxx APP_ID=jp.example.app ./scripts/pad-fast-install.sh
#   (通常は make pad-fast から呼ぶ)
#
# 依存: Android SDK (adb) / JDK (java, pack 変更時のみ) / python (zip 抽出) / curl (bundletool 取得)。
set -euo pipefail

BUILD_SYSTEM_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$BUILD_SYSTEM_DIR"
# shellcheck source=scripts/pad-common.sh
source "$BUILD_SYSTEM_DIR/scripts/pad-common.sh"

PROJECT_DIR="${PROJECT_DIR:-$BUILD_SYSTEM_DIR}"
BUILD_ROOT="$PROJECT_DIR/build/android"
BUNDLETOOL_VERSION="${BUNDLETOOL_VERSION:-1.17.2}"
BUNDLETOOL_JAR="$BUILD_SYSTEM_DIR/tools/bundletool-all-${BUNDLETOOL_VERSION}.jar"
PYTHON="${PYTHON:-python}"

# APP_ID: make から渡る。単体実行時は app-config.json から拾う (簡易 grep)。
APP_ID="${APP_ID:-}"
if [ -z "$APP_ID" ]; then
  APP_ID="$(grep -o '"applicationId"[[:space:]]*:[[:space:]]*"[^"]*"' "$PROJECT_DIR/app-config.json" 2>/dev/null \
            | sed 's/.*"\([^"]*\)"$/\1/' || true)"
fi
[ -n "$APP_ID" ] || { echo "ERROR: APP_ID を解決できません (env APP_ID か app-config.json)"; exit 1; }

ADB="$(pad_resolve_adb)"
[ -n "$ADB" ] || { echo "ERROR: adb が見つかりません (ANDROID_HOME / local.properties の sdk.dir)"; exit 1; }
echo "adb=$ADB  APP_ID=$APP_ID"

echo "== [1/5] assembleDebug (-PpadLocalTesting) =="
./gradlew :app:assembleDebug -PpadLocalTesting

APK="$(find "$BUILD_ROOT/app/outputs/apk/debug" -name '*.apk' 2>/dev/null | head -n1 || true)"
[ -n "$APK" ] || { echo "ERROR: debug APK が見つかりません ($BUILD_ROOT/app/outputs/apk/debug)"; exit 1; }

echo "== [2/5] install base APK =="
echo "APK: $APK"
if ! "$ADB" install -r -d "$APK"; then
  echo
  echo "ERROR: install -r に失敗しました。署名不一致の可能性があります。"
  echo "       once: make reinstall もしくは make pad-install でクリーン導入してください。"
  exit 1
fi

# ---- pack 差分検出 ----
LT="$(pad_external_lt_dir "$APP_ID")"
MARK="$(pad_marker_dir "$BUILD_ROOT")"
mkdir -p "$MARK"
"$ADB" shell "mkdir -p '$LT'" >/dev/null 2>&1 || true

mapfile -t PACKS < <(pad_pack_names "$BUILD_ROOT")
CHANGED=()
declare -A HASH
echo "== [3/5] pack 差分チェック =="
for name in "${PACKS[@]:-}"; do
  [ -z "$name" ] && continue
  h="$(pad_pack_hash "$BUILD_ROOT/assetpacks/$name/src/main/assets")"
  HASH["$name"]="$h"
  ondev="$("$ADB" shell "[ -f '$LT/${name}-master.apk' ] && echo yes || echo no" | tr -d '\r ')"
  prev="$(cat "$MARK/$name.sha" 2>/dev/null || true)"
  if [ "$h" = "$prev" ] && [ "$ondev" = "yes" ]; then
    echo "  - $name: 変更なし (skip)"
  else
    if [ "$ondev" != "yes" ]; then
      echo "  - $name: 端末未配置 → push"
    else
      echo "  - $name: 内容変更 → 再 push"
    fi
    CHANGED+=("$name")
  fi
done

if [ "${#CHANGED[@]}" -gt 0 ]; then
  echo "== [4/5] 変更 pack を再ビルドして push: ${CHANGED[*]} =="
  echo "  (AAB を作り直します。変更が無い pack は push しません)"
  ./gradlew :app:bundleDebug
  AAB="$(find "$BUILD_ROOT/app/outputs/bundle/debug" -name '*.aab' 2>/dev/null | head -n1 || true)"
  [ -n "$AAB" ] || { echo "ERROR: AAB が見つかりません"; exit 1; }

  if [ ! -f "$BUNDLETOOL_JAR" ]; then
    echo "  bundletool ${BUNDLETOOL_VERSION} をダウンロード..."
    mkdir -p "$BUILD_SYSTEM_DIR/tools"
    curl -L -o "$BUNDLETOOL_JAR" \
      "https://github.com/google/bundletool/releases/download/${BUNDLETOOL_VERSION}/bundletool-all-${BUNDLETOOL_VERSION}.jar"
  fi

  APKS="$BUILD_ROOT/app/outputs/bundle/pad-fast.apks"
  rm -f "$APKS"
  DEBUG_KS="$HOME/.android/debug.keystore"
  SIGN=()
  [ -f "$DEBUG_KS" ] && SIGN=(--ks="$DEBUG_KS" --ks-pass=pass:android --ks-key-alias=androiddebugkey --key-pass=pass:android)
  java -jar "$BUNDLETOOL_JAR" build-apks --bundle="$AAB" --output="$APKS" --local-testing "${SIGN[@]}"

  TMP="$BUILD_ROOT/app/outputs/bundle/pad-fast-extract"
  rm -rf "$TMP"; mkdir -p "$TMP"
  for name in "${CHANGED[@]}"; do
    entry="$("$PYTHON" -c "import zipfile,sys
z=zipfile.ZipFile(sys.argv[1])
print(next((n for n in z.namelist() if n.split('/')[-1]==sys.argv[2]), ''))" "$APKS" "${name}-master.apk")"
    if [ -z "$entry" ]; then
      echo "  WARN: ${name}-master.apk が .apks 内に見つかりません (skip)"
      continue
    fi
    "$PYTHON" -c "import zipfile,sys
zipfile.ZipFile(sys.argv[1]).extract(sys.argv[2], sys.argv[3])" "$APKS" "$entry" "$TMP"
    echo "  push ${name}-master.apk ..."
    "$ADB" push "$TMP/$entry" "$LT/${name}-master.apk" >/dev/null
    # 同 versionCode (1/1) では既存展開が再利用されてしまうので、展開先を消して再展開させる。
    "$ADB" shell "run-as $APP_ID rm -rf files/assetpacks/${name}" >/dev/null 2>&1 || true
    echo "${HASH[$name]}" > "$MARK/$name.sha"
  done
else
  echo "== [4/5] pack 変更なし → 端末の既存データを再利用 =="
fi

# ---- 現行 config に無い古い pack を端末から掃除 (rename/削除の後始末) ----
CURSET=" ${PACKS[*]:-} "
for d in $("$ADB" shell "run-as $APP_ID ls files/assetpacks 2>/dev/null" | tr -d '\r'); do
  [ "$d" = "_tmp" ] && continue
  case " $CURSET " in
    *" $d "*) ;;  # 現行 pack → 残す
    *)
      echo "  prune: 古い展開 pack を削除 files/assetpacks/$d"
      "$ADB" shell "run-as $APP_ID rm -rf files/assetpacks/$d" >/dev/null 2>&1 || true
      ;;
  esac
done

echo "== [5/5] 起動 =="
"$ADB" shell monkey -p "$APP_ID" -c android.intent.category.LAUNCHER 1 >/dev/null
echo
echo "完了。データ未変更なら pack は再 push していません。"
echo "ログ: adb logcat -s BootstrapActivity:* PadAssetResolver:*"
