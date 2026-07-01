#!/bin/bash
#
# Play Store アップロード用の AAB (base + 全 PAD asset pack) をビルドするスクリプト。
#
# 使い方:
#   PROJECT_DIR=d:/work/game/xxx ./scripts/pad-build-aab.sh
#   VARIANT=Release ./scripts/pad-build-aab.sh   # 既定は Release
#
# Release 署名は local.properties の RELEASE_STORE_FILE 等 (無ければ同名環境変数)。
# 生成された 1 つの .aab をそのまま Play Console にアップロードすれば、asset pack は
# Play が配信タイプ (on-demand/fast-follow/install-time) に従って自動配信する。
set -euo pipefail

BUILD_SYSTEM_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$BUILD_SYSTEM_DIR"

PROJECT_DIR="${PROJECT_DIR:-$BUILD_SYSTEM_DIR}"
VARIANT="${VARIANT:-Release}"

echo "== AAB をビルド (bundle${VARIANT}) =="
./gradlew ":app:bundle${VARIANT}"

BUNDLE_DIR="$PROJECT_DIR/build/android/app/outputs/bundle"
AAB="$(find "$BUNDLE_DIR" -name '*.aab' 2>/dev/null | head -n1 || true)"
if [ -z "${AAB:-}" ]; then
  echo "ERROR: AAB が見つかりません ($BUNDLE_DIR)"; exit 1
fi

echo
echo "AAB: $AAB"
ls -lh "$AAB"
echo
echo "次の手順:"
echo "  1. Play Console > 対象アプリ > テスト/製品版 > 新しいリリースを作成"
echo "  2. 上記 .aab をアップロード (asset pack は AAB に内包済み)"
echo "  3. Play App Signing が有効であること (PAD の本番配信に必須)"
echo
echo "内容確認 (任意): bundletool build-apks --bundle=\"$AAB\" --output=/tmp/x.apks で split 一覧を確認できます。"
