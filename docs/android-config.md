# app-config.json — Android 版の設定

プロジェクトフォルダの `app-config.json` で、アプリ ID・バージョン・SDK のほか、組み込むプラグイン
(`cmake`) と取り込む資材 (`assetPack`) を指定します。`app/build.gradle` がこのファイルを読みます。

`cmake` / `assetPack` / パス文字列の `${...}` 展開は、krkrz_ios / krkrz_linux / krkrz_gdk / krkrz_web と
共通の書式です。共通部分の説明は公開サイトの
[外枠リポジトリの共通設定 (cmake / assetPack)](https://wamsoft.github.io/krkrz_dev/topics/core/platform_config/)
にあります。このページは Android 版だけの項目と、Android での動き方を説明します。

## 全体の例

リポジトリ直下の `app-config.json` がサンプル兼ひな形です。

```jsonc
{
    "applicationId": "jp.wamsoft.krkrz.sample",
    "versionCode": 1,
    "versionName": "1.0",
    "compileSdk": 36,
    "targetSdk": 36,
    "minSdk": 29,
    "ndkVersion": "28.2.13676358",
    "apk": { "baseName": "mygame" },
    "manifestPlaceholders": { "screenOrientation": "landscape" },
    "cmake": {
        "pluginFolders": [
            "${BUILD_SYSTEM_DIR}/app/src/main/cpp",
            "${KRKRZ_BASE}/krkrz_dev/src/plugins"
        ],
        "plugins": ["json", "csvParser", "saveStruct"],
        "staticPlugins": []
    },
    "assetPack": {
        "baseFolder": "${PROJECT_DIR}",
        "resOverride": "android-res",
        "sources": [
            { "type": "mirror", "from": "${PROJECT_DIR}/resource", "to": "" },
            { "type": "mirror", "from": "${KRKRZ_BASE}/krkrz_dev/src/core/resource", "to": "" },
            { "type": "mirror", "from": "data", "exclude": ["**/*.bak", "**/*.psd"] },
            { "type": "flatten", "from": "archive", "include": ["*.xp3"] }
        ]
    }
}
```

## アプリの基本情報

| キー | 内容 |
|---|---|
| `applicationId` | アプリの一意な識別子 (パッケージ名) |
| `versionCode` | 内部のバージョン番号 (整数。リリースごとに増やす) |
| `versionName` | ユーザーに見せるバージョン文字列 |
| `compileSdk` / `targetSdk` / `minSdk` | コンパイルに使う SDK / 対象 SDK / 最小動作 SDK |
| `ndkVersion` | 使う NDK のバージョン |

アプリ名は `app/src/main/res/values/strings.xml` の `app_name` です (`resOverride` で差し替えられます。
[カスタマイズ](android-customize.md) を参照)。

## apk — 出力ファイル名

```jsonc
"apk": { "baseName": "mygame" }   // 既定: "app"
```

- APK のファイル名が `<baseName>-<versionName>-<variant>.apk` になります
  (例: `mygame-1.0-debug.apk` / `mygame-1.0-release.apk` / `mygame-1.0-releaseDebugSigned.apk`)。
  出力先 (`${PROJECT_DIR}/build/android/app/outputs/apk/<variant>/`) は変わりません
- AAB は AGP の制約で `app-<variant>.aab` のままです
- 省略時は AGP の既定 (`app-<variant>.apk` / `app-<variant>.aab`)
- 出力を別の場所に集めたい場合は、`PROJECT_DIR` を切り替えるか、スクリプトで
  `${PROJECT_DIR}/build/android/app/outputs/apk/...` からコピーしてください

## manifestPlaceholders — AndroidManifest の値の差し替え

`AndroidManifest.xml` の `${KEY}` 形式のプレースホルダを置き換えます。画面の向き・API キー・
Deep Link のホスト名など、Manifest に直接書く値を Manifest を編集せずにアプリごとに切り替えられます。

```json
"manifestPlaceholders": {
    "screenOrientation": "landscape",
    "deepLinkHost": "example.com"
}
```

```xml
<activity
    android:name="..."
    android:screenOrientation="${screenOrientation}" >
    <intent-filter>
        <data android:scheme="https" android:host="${deepLinkHost}" />
    </intent-filter>
</activity>
```

省略すると何も置き換えないので、プレースホルダを使わない既定の Manifest はそのままビルドできます。

## cmake — 組み込むプラグイン

書式は[共通設定](https://wamsoft.github.io/krkrz_dev/topics/core/platform_config/)のとおりです。
Android では次のように動きます。

- ビルド前と CMake の configure 前に `generateMyappCmake` タスクがこのセクションを読み、
  `${PROJECT_DIR}/build/android/app/generated/cmake/myapp.cmake` を作ります。CMake
  (`app/src/main/cpp/CMakeLists.txt`) は `-DMYAPP_CMAKE_FILE=<絶対パス>` でこれを `include()` します
- 生成物は `build/android/` の下なので、クリーンビルドで消え、コミットの対象にもなりません。
  直接編集しても次のビルドで上書きされるので、変更は `app-config.json` で行ってください
- `staticPlugins` に書いたものは `libkrkrz.so` に静的リンクされます
- このリポジトリの中のプラグイン (`app/src/main/cpp` の Android 専用プラグインなど) を使う場合は
  `${BUILD_SYSTEM_DIR}/app/src/main/cpp` を `pluginFolders` に書きます
- `cmake` を省略すると `generateMyappCmake` は何もしません

## assetPack — 資材の取り込み

書式は[共通設定](https://wamsoft.github.io/krkrz_dev/topics/core/platform_config/)のとおりです。
Android では `packAssets` タスクが `preBuild` の前に走り、資材を
`${PROJECT_DIR}/build/android/app/generated/assetPack/` に集めて、`assets.srcDirs` 経由で APK に入れます。

- **エンジンの必須資材 (`${KRKRZ_BASE}/krkrz_dev/src/core/resource`) も `sources` の 1 項目として
  書きます**。`sources` は書いた順に処理して先勝ちなので、プロジェクトフォルダの `resource/` を
  その前に書けば、個別のファイルを差し替えられます
- 同じ置き先の衝突は Gradle のログに出ます

  ```
  > Task :app:packAssets
  [assetPack] duplicate skipped (first wins): assets/config.cf
              kept   : <PROJECT_DIR>/resource/config.cf
              skipped: <KRKRZ_BASE>/krkrz_dev/src/core/resource/config.cf
  [assetPack] 15 file(s) total: 8 copied, 7 unchanged, 0 removed
  ```

- 取り込みだけを試すには `./gradlew :app:packAssets`。入力が変わらなければ `UP-TO-DATE` で飛ばされます
  (やり直すときは `--rerun-tasks`)

### app/src/main/assets/ との衝突

先勝ちが効くのは `assetPack` の `sources` どうしだけです。`app/src/main/assets/` (AGP の既定の
assets フォルダ) に置いたファイルと `assetPack` が作るファイルに同じ名前があると、AGP の
`mergeAssets` が **`DuplicateDataException` でビルドを止めます**。AGP には片方を優先させる
指定が無いので、次のどちらかで避けてください。

- 衝突するファイルを `app/src/main/assets/` から消し、必要なら `assetPack.sources` 側へ移す
- `assetPack.sources[].exclude` で `assetPack` 側から除く

`app/src/main/assets/` はテンプレートとして空にしてあります (`.gitignore` の対象)。資材は
`assetPack` で取り込んでください。

### resOverride — アイコン・res の上書き

`baseFolder` の下に Android の `res/` の規約どおりのフォルダを置き、`resOverride` で指定すると、
`app/src/main/res/` の上に重ねて **ファイル単位 / values の要素単位で上書き**できます。各 buildType の
`res.srcDirs` に足す形なので、AGP のリソースのマージがそのまま使われます。

```jsonc
"assetPack": {
    "baseFolder": "${PROJECT_DIR}",
    "resOverride": "android-res",
    // 複数も指定できる (後ろほど優先): "resOverride": ["base-res", "brand-res"],
    "sources": [ ... ]
}
```

```
android-res/
├── mipmap-mdpi/ic_launcher.webp        ← app/src/main/res の同名を上書き
├── mipmap-hdpi/ic_launcher.webp
├── mipmap-xhdpi/ic_launcher.webp
├── mipmap-xxhdpi/ic_launcher.webp
├── mipmap-xxxhdpi/ic_launcher.webp
├── mipmap-anydpi-v26/ic_launcher.xml   ← アダプティブアイコン (必須。カスタマイズを参照)
├── drawable/ic_launcher_foreground.xml
└── values/strings.xml                  ← <string name="app_name">..</string> など要素単位で上書き
```

- 値は `baseFolder` からの相対フォルダ名 (またはその配列)
- フォルダ構成は Android の `res/` の規約 (`mipmap-*` / `drawable*` / `values*` など) に従います。
  PNG を平らに並べる形には対応しません
- 全ての buildType (debug / release / releaseDebugSigned) に同じ上書きが効きます
- `values` は要素単位でマージされるので、`app_name` だけ書いた `strings.xml` を置けば他の文字列は残ります

### padPacks — Play Asset Delivery

APK に入らない大きなデータ (数 GB の xp3 や動画) は、`padPacks` で Play Asset Delivery の asset pack として
配信します。取り込みの書き方 (`mirror` / `flatten` / `include` / `exclude` / `to`) は `assetPack` と同じです。
詳しくは [Play Asset Delivery](android-play-asset-delivery.md) を参照してください。

## プロジェクトフォルダの指定 (PROJECT_DIR / APP_CONFIG_FILE)

通常は環境変数 `PROJECT_DIR` を使います。`app-config.json`・資材・ビルド出力がすべてその下に揃います。
未指定ならこのリポジトリ自身 (サンプル) です。

```bash
make build PROJECT_DIR=/path/to/mygame
# または
export PROJECT_DIR=/path/to/mygame
./gradlew assembleDebug
```

`app-config.json` の場所だけを変えたいとき (同じプロジェクトフォルダで設定だけを切り替えるなど) は
`APP_CONFIG_FILE` を使います。優先順は次のとおりです。

1. `local.properties` の `APP_CONFIG_FILE=...`
2. 環境変数 `APP_CONFIG_FILE`
3. 指定が無ければ `${PROJECT_DIR}/app-config.json`

```properties
# local.properties
APP_CONFIG_FILE=C:/work/mygame/app-config.json
# APP_CONFIG_FILE=${KRKRZ_BASE}/mygame/app-config.json   (展開も使える)
```

- `${BUILD_SYSTEM_DIR}` / `${PROJECT_DIR}` / 環境変数を展開します。相対パスは `PROJECT_DIR` 基準です
- 指定したファイルが無い場合は、その場でビルドがエラーで止まります

**MSYS2 の make では**外のシェルの環境変数が渡らないことがあるので、`make` に渡すときは
`make build PROJECT_DIR=...` のように引数で指定するのが確実です。`./gradlew` を直接呼ぶ場合は
環境変数で問題ありません。

`local.properties` (SDK のパス・署名情報) はビルド環境の設定なので、プロジェクトフォルダではなく
このリポジトリ直下のものだけを読みます。

プロジェクトを切り替えた直後は、`${BUILD_SYSTEM_DIR}/app/.cxx/` (AGP の仕様で場所が固定) に前の
プロジェクトの CMake / vcpkg のキャッシュが残り、リンクエラーになることがあります。切り替えたら
一度 `make clean-cmake` してください。
