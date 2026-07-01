# Android Play Asset Delivery (PAD) 対応

大容量の案件データ (xp3 アーカイブ・動画) を **Google Play Asset Delivery** で配信するための仕組みと手順。

## なぜ必要か

- 案件データは数 GB になりうる (xp3 の合計が数 GB規模)。base APK / install-time の上限を超え、**そのままでは Play Store で配信できない**。
- PAD の **fast-follow / on-demand** パックは、端末の内部ストレージに **展開済みの実ファイル**として置かれ、`AssetPackManager.getPackLocation(name).assetsPath()` で絶対パスが取れる。
- KrkrZ エンジンは既に「絶対パスのデータルート」(`setAssetCacheDir`) を受け取ると、そこを filesystem 直読み (mmap/seek 安い) する。**このパスを PAD の展開先に向けるだけ**で、native もデータコピーも不要で高速に読める。
- 参考: 従来は「全部 assets 同梱 → 起動時 `<filesDir>/assets/` へコピー」していた ([android-jni-plugin.md] とは別系統の Bootstrap 機構)。PAD ではこのコピーが不要になる。

## 配信タイプの違い (重要)

| deliveryType | 配信タイミング | ランタイム参照 | 本テンプレートでの扱い |
|---|---|---|---|
| `install-time` | インストール時に同梱 | **AssetManager 名前空間** (実ファイル展開されない) | 既存 Bootstrap の assets コピー経路が拾う |
| `fast-follow` | インストール直後に Play が自動DL | `assetsPath()` (実ファイル) | fetch パックとして on-demand と同一経路 |
| `on-demand` | アプリが `fetch()` した時 | `assetsPath()` (実ファイル) | fetch パックとして扱う (既定) |

- サイズ目安 (2024-2025): base/install-time モジュールは各 500MB、install-time 合算 4GB。**on-demand + fast-follow は合算 30GB** まで。個々のパックは 1GB 前後に収めるのが無難 → **複数パックに分割する**。
- `install-time` パックへ xp3 を入れても実ファイル展開されず AAssetManager 経由 (遅い) になるため、**大容量は fast-follow / on-demand を推奨**。

## app-config.json スキーマ

既存の `assetPack` (= APK 同梱アセット。小物用) は**そのまま**。兄弟に **`padPacks`** 配列を追加する。ソース規則 (`mirror`/`flatten` + `include`/`exclude`/`to`) は `assetPack` と共通。

```jsonc
"assetPack": {
  "baseFolder": "${PROJECT_DIR}",
  "resOverride": "setup/andspec/res",
  "sources": [
    { "type": "mirror", "from": "setup/nxresource", "to": "" }   // config.cf / fonts / messages 等の小物のみ
  ]
},
"padPacks": [
  {
    "name": "krkrz_core",              // gradle module 名 & packName。[a-z0-9_]+ のみ
    "deliveryType": "on-demand",       // install-time | fast-follow | on-demand (既定 on-demand)
    "baseFolder": "${PROJECT_DIR}",
    "sources": [
      { "type": "flatten", "from": "build/_temp/archive",
        "include": ["data.xp3", "others.xp3", "steam.xp3", "debug.xp3"] }
    ]
  },
  {
    "name": "krkrz_video",
    "deliveryType": "on-demand",
    "baseFolder": "${PROJECT_DIR}",
    "sources": [
      { "type": "mirror", "from": "data", "include": ["video/*"] }  // 動画は loose file 配置
    ]
  }
]
```

### パック分割方針 (例)

| パック | 内容 | ~サイズ |
|---|---|---|
| base `assetPack` (APK同梱) | config.cf / フォント / gamecontrollerdb / messages | <2MB |
| `krkrz_core` | data + others + steam + debug | ~1.0GB |
| `krkrz_images` | fgimage + bgimage | ~0.9GB |
| `krkrz_events` | evimage + evimage2 + voice | ~0.65GB |
| `krkrz_video` | data/video/* (loose) | ~0.76GB |

> **全パック on-demand** でも、初回起動時に Bootstrap が**全部まとめて fetch** して進捗表示 → 完了後に起動する (下記ランタイム)。分割は主に**サイズ上限と DL 粒度**のため。エンジンからは統合された単一ルートに見える。

## ビルドの仕組み

1. **`settings.gradle`**: `app-config.json` の `padPacks` を読み、各パックにつき `${PROJECT_DIR}/build/android/assetpacks/<name>/` に **asset pack モジュールを生成** (`build.gradle` + 空の `src/main/assets/`) して `include` する。asset pack プラグインの版は root `build.gradle` の `com.android.asset-pack` alias から解決。
2. **`app/build.gradle`**:
   - `android.assetPacks = [":<name>", …]` を設定 (padPacks が無ければ未設定＝従来どおり)。
   - `implementation 'com.google.android.play:asset-delivery:2.3.0'` (ランタイム)。
   - 既存 `packAssets` のコピー処理を共通クロージャ化し、パックごとに **`packPad_<name>`** タスクを登録 → 各モジュールの `src/main/assets/` へ mirror/flatten コピー。
   - **`generatePadRes`**: fast-follow/on-demand のパック名を `res/values/pad_packs.xml` の string-array `pad_fetch_packs` に出力 (ランタイムが参照)。
   - `packPad_*` は各 asset pack モジュールの `generateAssetPackManifest` より前に走るよう依存付け済み。
3. サンプル (このリポジトリ単体・`padPacks` 無し) は**完全に従来動作** (後方互換)。

## ランタイム解決

起動時 `BootstrapActivity` がデータルートを優先順で解決し、`KrkrzActivity` へ Intent extra `assetRoot` で渡す。native (`setAssetCacheDir`) は無改修。

1. **PAD fetch パックあり** (`R.array.pad_fetch_packs` 非空): `PadAssetResolver.ensurePacksReady()` が
   - 各パックの `getPackLocation()` を確認、未展開は `fetch()` して `AssetPackStateUpdateListener` で **既存 ProgressBar** を byte 集計で駆動 (Wi-Fi 待ち等は `showConfirmationDialog`)。
   - 全 COMPLETED 後、`assetsPath()` を収集。
   - `<filesDir>/assets/` (base+install-time のコピー) と各パスを **`<filesDir>/padroot/` に毎起動 symlink 統合** (コピー無し, 先勝ち) → その絶対パスをルートに。
2. **PAD 無し**: 従来の `<filesDir>/assets/` を単一ルートに。
3. **どちらも無い**: 何も渡さず engine のレガシー AssetManager 経路。

> `assetsPath()` は更新でパックが移動しうるため**キャッシュ禁止**。padroot は毎起動作り直す。

主なファイル: `PadAssetResolver.java` / `BootstrapActivity.java` / `KrkrzActivity.java`。

## ローカル検証 (Play を通さない)

```bash
PROJECT_DIR=d:/work/game/xxx ./scripts/pad-local-install.sh
```

- 内部で AAB をビルド → `bundletool build-apks --local-testing` → `install-apks`。
- `--local-testing` により端末上で AssetPackManager がローカル配置のパックを解決し、**本番と同じランタイム経路がオフラインで動く**。
- 注意: `--local-testing` では **fast-follow も自動DLされず on-demand と同様に fetch が必要** (本テンプレートは全 fetch パックを起動時に取得するので問題なし)。
- ログ: `adb logcat -s BootstrapActivity:* PadAssetResolver:*` で各パックの `assetsPath` と padroot を確認。
- `VARIANT=Release ./scripts/pad-local-install.sh` で Release AAB を検証可 (要 Release 署名)。

## ポータブル手動インストーラ (このリポジトリ無しで配布)

ビルド環境の無い任意の PC (Java + adb だけ) から手動インストールできる一式を作る:

```bash
PROJECT_DIR=d:/work/game/xxx ./scripts/pad-package-installer.sh   # 既定 Debug / VARIANT=Release も可
```

- 出力 `${PROJECT_DIR}/build/android/pad-installer/` に **app.apks (PAD パック同梱・署名済) + bundletool.jar + install.sh / install.bat + README.txt** を同梱。
- このフォルダを丸ごと配布 → 配布先で `./install.sh` (Linux/macOS/Git Bash) か `install.bat` (Windows) を実行するだけ。**リポジトリ/ビルド環境不要**。
- `.apks` は事前に build-apks --local-testing 済みなので、配布先は署名鍵不要・`bundletool install-apks` が base+config split の導入と asset pack のローカル push を行う。
- `.apks` は数 GB (zip 圧縮効果ほぼ無し)。そのまま転送する。

## Play Store 配信

```bash
PROJECT_DIR=d:/work/game/xxx ./scripts/pad-build-aab.sh
```

- 生成された **1 つの .aab** を Play Console にアップロードするだけ。asset pack は AAB に内包され、Play が配信タイプに従って自動配信する。
- **Play App Signing 有効が必須** (PAD の本番 fetch に必要)。
- 事前準備 (アイコン・ストア掲載情報・プライバシーポリシー等) は案件側の「Androidリリース準備項目.txt」を参照。

## トラブルシュート

- **パックが空でバンドルエラー**: `build/_temp/archive` の xp3 が未生成のまま `packPad_*` が走ると空になる。先に xp3 をビルドしてから AAB を作る。
- **`assetsPath()` が null**: パックが COMPLETED 前。local-testing でも `install-apks` 経由でないと展開されない (素の `adb install apk` では install-time/fetch パックは来ない)。
- **`app-config.json` を変えたのに反映されない**: 端末側 cache/pack は再インストールで更新。`scripts/pad-local-install.sh` を再実行する。
- **symlink 失敗**: 端末/FS が内部ストレージの symlink を拒否する場合 (通常無い)。その場合は native をマルチルート対応へ拡張する (保険案)。
- **パック名の制約**: `[a-z0-9_]+` のみ (gradle module 名になるため)。

[android-jni-plugin.md]: ./android-jni-plugin.md
