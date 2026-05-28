# 吉里吉里Z Android版 プロジェクト

このプロジェクトは、吉里吉里Z (Krkrz) を Android で
動作させるためのプロジェクトテンプレートです

## セットアップ

### 0. KRKRZソース配置準備

ライブラリ参照に vcpkg を使っているので、
環境変数 VCPKG_ROOT に vcpkg のルートフォルダを指定する必要があります

吉里吉里Zのソース参照用に、環境変数 KRKRZ_BASE に 
krkrz_dev （krkrz + plugin 共通プロジェクトフォルダ）がある場所を指定しておきます。

以下のフォルダを参照するように app/src/main/cpp/CMakeLists.txt が記載されています

吉里吉里本体             ${KRKRZ_BASE}/krkrz_dev/src/core
吉里吉里プラグインソース ${KRKRZ_BASE}/krkrz_dev/src/plugins 

同時リンクするプラグインの構成は `app-config.json` の `cmake` セクションで指定します
(後述「CMake プラグイン構成 (cmake)」参照)。ビルド時にこの設定から
`${PROJECT_DIR}/build/android/app/generated/cmake/myapp.cmake` (サンプル動作時は
リポジトリ直下の `build/android/app/generated/cmake/`) が自動生成され、
CMake から取り込まれます。生成物は `build/android/` 配下なのでクリーンビルドで
自動的に消えます。直接編集せず JSON を編集してください。

### 0.0. ビルドシステムフォルダと案件フォルダの分離 (BUILD_SYSTEM_DIR / PROJECT_DIR)

このリポジトリは「共通のビルドシステム」、ビルド対象の案件 (app-config.json + リソース)
は別フォルダ、という構成を取れます。

| 変数 | 意味 |
|------|------|
| `BUILD_SYSTEM_DIR` | このリポジトリのルート (gradle/cmake/cpp/java のソースが置いてある場所)。常に固定で、Makefile / settings.gradle が自動で解決します |
| `PROJECT_DIR` (環境変数) | 案件フォルダ。`${PROJECT_DIR}/app-config.json` が参照され、`${PROJECT_DIR}/build/android/` 以下にビルド出力 (`app/` と `root/`) が掃き出されます |

`PROJECT_DIR` 未設定時は `BUILD_SYSTEM_DIR` にフォールバックするので、このリポジトリ
単体で「サンプル案件」としてビルドできます (リポジトリ同梱の `app-config.json` /
`resource/` がそのまま使われる)。

```bash
# サンプル動作 (このリポジトリ単体)
make build

# 案件ビルド
export PROJECT_DIR=/path/to/myproject
make build      # ${PROJECT_DIR}/app-config.json を読み、
                # ${PROJECT_DIR}/build/android/app/outputs/apk/... に APK 出力
```

`app-config.json` 内の `${...}` 展開と相対パス解決は **`PROJECT_DIR` 基準** です。
ビルドシステム側 (このリポジトリ内のソース) を指したい場合は `${BUILD_SYSTEM_DIR}/...`
を明示してください (例: Android 専用プラグインの `app/src/main/cpp` 配下)。

`local.properties` (SDK パス・署名情報) は **案件横断のビルド環境設定** なので、
案件側ではなく `BUILD_SYSTEM_DIR` 直下のみが参照されます。

> **案件を切り替えた直後の注意**: AGP の仕様上 `.cxx/` だけは
> `${BUILD_SYSTEM_DIR}/app/.cxx/` 固定で生成されます。前案件の CMake / vcpkg
> キャッシュが残っているとリンクエラーになることがあるので、案件切り替え時は
> 一度 `make clean-cmake` を流すのを推奨します。

### 1. ライブラリの配置

SDLの `.aar` ファイルを `app/libs/` ディレクトリに配置してください。

* `SDL3-x.x.x.aar` (SDL3 ライブラリ)

※ `app/build.gradle` の設定により、`libs` フォルダ内のすべての `.aar` ファイルが自動的に読み込まれます。

install-sdl-aar.sh で指定バージョンの SDLをダウンロードして配置できます

### 2. リソース・スクリプトの配置

吉里吉里Zの実行に必要なデータは `app/src/main/assets/` 以下に配置します。

* **スクリプト・データ**: `app/src/main/assets/data/` 以下に配置するとその中の `startup.tjs`から起動します

外部のプロジェクトフォルダから assets にコピーしたい場合は、後述の
[assetPack によるアセット自動コピー](#assetpack-によるアセット自動コピー) を参照してください。

## assetPack によるアセット自動コピー

リポジトリ外にあるプロジェクト一式 (data フォルダや tjs スクリプト群)
を、ビルド時に自動で `assets/` に取り込む仕組みです。
`app-config.json` に `assetPack` セクションを追加すると、
`packAssets` タスクが自動で `preBuild` 前に走り、指定したファイルを
`${PROJECT_DIR}/build/android/app/generated/assetPack/` に書き出して
`assets.srcDirs` 経由で APK に含めます。

KrkrZ 本体の必須資材である `${KRKRZ_BASE}/krkrz_dev/src/core/resource` も
`assetPack.sources` の 1 エントリとして取り込みます (以前は `assets.srcDirs` に
直接追加していましたが、優先度制御と同名ファイル衝突回避のため `assetPack`
に一本化しました)。`assetPack` の `sources` は **記載順に処理され、先勝ち**
なので、自プロジェクト側のファイルを先に書けば本体側を上書きできます。

### 設定例

リポジトリ同梱の `app-config.json` がそのまま設定例です。本体 resource を
ルートに展開しつつ、自プロジェクト側の `resource/` で個別ファイルを
先勝ちで上書きする構成になっています。

```jsonc
{
    "applicationId": "jp.wamsoft.krkrz.sample",
    "versionCode": 1,
    "versionName": "1.0",
    "compileSdk": 36,
    "targetSdk": 36,
    "minSdk": 29,
    "ndkVersion": "28.2.13676358",
    "assetPack": {
        "baseFolder": "${KRKRZ_BASE}/krkrz_dev/src/core",
        "sources": [
            {
                "comment": "Override files from this project's own resource folder (wins on conflict)",
                "type": "mirror",
                "from": "${PROJECT_DIR}/resource",
                "to": ""
            },
            {
                "comment": "KrkrZ engine base resource (mandatory)",
                "type": "mirror",
                "from": "${KRKRZ_BASE}/krkrz_dev/src/core/resource",
                "to": ""
            },
            {
                "comment": "Game data tree under baseFolder/data",
                "type": "mirror",
                "from": "data",
                "include": ["**/*"],
                "exclude": ["**/*.bak", "**/*.psd"]
            },
            {
                "comment": "Pack .xp3 archives flatly under assets root",
                "type": "flatten",
                "from": "archive",
                "include": ["*.xp3"]
            }
        ]
    }
}
```

> `comment` のような未知のキーは無視されるので、各 source の意図を
> 書き残しておく用途で自由に使えます。

### パラメータ

| キー | 必須 | 内容 |
|------|------|------|
| `baseFolder` | 相対 `from` を使うなら必須 | コピー元のベースフォルダ。`${PROJECT_DIR}` 基準の相対パス、絶対パス、`${ENV_VAR}` / `${BUILD_SYSTEM_DIR}` / `${PROJECT_DIR}` 展開が利用可能 |
| `transfer` | 省略可 | 転送方式。`"copy"` (既定) または `"hardlink"` |
| `resOverride` | 省略可 | `baseFolder` 配下のフォルダ名 (string) または配列。詳細は後述 |
| `sources` | ◯ | コピー定義の配列。記載順に処理されます |
| `sources[].type` | ◯ | `"mirror"` または `"flatten"` |
| `sources[].from` | ◯ | フォルダパス。相対なら `baseFolder` 起点、絶対 (`${ENV_VAR}` 展開後に絶対) ならそのまま参照 |
| `sources[].include` | 省略可 | Ant パターンの配列 (例: `"**/*.png"`)。既定 `["**/*"]` |
| `sources[].exclude` | 省略可 | Ant パターンの配列。既定 `[]` |
| `sources[].to` | 省略可 | assets 配下の出力先パス。省略時は `mirror`+相対 `from`=`from` と同じ、`mirror`+絶対 `from`=`from` の末尾フォルダ名、`flatten`=`""` (assets 直下)。`""` を明示すれば常に assets 直下に展開 |

### type の動作

- **`mirror`**: `from` 配下を Ant パターンでフィルタし、
  ディレクトリ構造を維持したまま `assets/<to>/` 以下にコピーします。
  例: `from: "data"` → `assets/data/...` /
  `from: "${KRKRZ_BASE}/krkrz_dev/src/core/resource"` → `assets/resource/...` /
  同上 + `to: ""` → `assets/...` (root に展開)
- **`flatten`**: 同様にフィルタしますが、ヒットしたファイルを
  サブディレクトリを捨てて `assets/<to>/` 直下にファイル名のみで
  配置します。

### 同名ファイルの衝突

複数の source や `flatten` のサブディレクトリ間で同じ出力パスに
書き込もうとした場合、**最初に現れたファイルが採用** され、
後続のものは Gradle の警告ログ付きでスキップされます。

```
> Task :app:packAssets
[assetPack] duplicate skipped (first wins): assets/config.cf
            kept   : <PROJECT_DIR>/resource/config.cf
            skipped: <KRKRZ_BASE>/krkrz_dev/src/core/resource/config.cf
[assetPack] 15 file(s) total: 8 copied, 7 unchanged, 0 removed
```

> **注意: `app/src/main/assets/` との衝突は first-wins 対象外**
>
> first-wins による吸収は **`assetPack` 内 (`sources[]` 同士)** に限られます。
> `app/src/main/assets/` (AGP 既定の assets srcDir) に置いたファイルと
> `assetPack` が生成するファイルに同名のものがあると、後段の AGP の
> `mergeAssets` で **`DuplicateDataException` でビルドが失敗** します。
> AGP 側に「片方を勝たせる」ような DSL は無いため、以下のいずれかで回避してください:
>
> - 衝突するファイルを `app/src/main/assets/` から削除し、必要なら
>   `assetPack.sources` 側 (例: `${PROJECT_DIR}/assets-extra` を mirror) に移す
> - `assetPack.sources[].exclude` で `assetPack` 側からそのファイルを除外する
>
> リポジトリの `app/src/main/assets/` は **テンプレートとして基本空** (`.gitignore` 対象)
> であり、運用上は `assetPack` 経由で資材を取り込むのが推奨フローです。

### 単独でのタスク実行

通常のビルド (`./gradlew assembleDebug` 等) では `preBuild` 経由で
自動実行されますが、コピーだけ確認したい場合は以下で単独実行できます。

```bash
./gradlew :app:packAssets
```

入力ファイルが変わらない限り `UP-TO-DATE` でスキップされます。
強制再実行したい場合は `--rerun-tasks` を付けてください。

### 再実行時のふるまい (差分更新)

`packAssets` は `Sync` ライクに差分のみ更新します。

- **ソース側で削除されたファイル** は出力ディレクトリからも自動的に削除されます。
- **内容に変更がないファイル** は再コピーされず、タイムスタンプもそのまま保持されます (後続の `mergeAssets` を不必要に走らせない)。
- **追加・変更されたファイルだけ** がコピーされます。
- 実行末尾に `N file(s) total: X copied, Y unchanged, Z removed` のサマリが出ます。

### transfer モード: hardlink (大容量アセット向け)

ソースとビルド出力先が **同じドライブ・同じファイルシステム上** にある場合、
`transfer: "hardlink"` を指定するとコピーの代わりに **NTFS / ext4 等のハードリンク**
を使ってファイル実体を共有します。数 GB のアセットでもディスクをほぼ消費せず、
転送も実質ゼロ時間になります。

```jsonc
"assetPack": {
    "baseFolder": "${KRKRZ_BASE}/myproject",
    "transfer": "hardlink",
    "sources": [ ... ]
}
```

挙動:

- ソース側でファイルを編集すると、ビルド出力側にも **即座に反映** されます
  (同じ inode を共有しているため)。
- `packAssets` の差分判定は引き続き mtime/size ベースなので、
  ソースを **置き換え (delete & re-create)** した場合は次回ランで再リンクされます。
- リンク作成に失敗したファイル (異ドライブ・ネットワーク FS・FAT 等
  ハードリンク非対応の環境) は **自動的にコピーへフォールバック** します。
  サマリに `X linked, Y copied (fallback)` と表示され、フォールバックが
  発生した場合は警告ログも出ます。
- Windows の NTFS では管理者権限不要で動作します。シンボリックリンクは
  対応しません (Windows では昇格が必要なため)。

注意:

- ソースを別途編集する運用 (例: ビルド前にスクリプトでソースを書き換える等)
  がある場合、出力側も同時に書き換わるため副作用に注意してください。
  そういった運用がある場合は既定の `copy` を使うのが安全です。
- 差分更新ロジック上、ソースを **置き換え (古いファイルを消して新ファイル作成)**
  したケースのみ再リンク対象になります。インプレース編集はリンクが
  自動追従するので何もしません (それで意図通り)。

### resOverride: アイコン・res の外部フォルダ上書き

`baseFolder` 配下に Android `res/` 規約に従ったフォルダを置いておけば、
`resOverride` でそれを `app/src/main/res/` の上に重ね、**個別ファイル単位 / values
要素単位で上書き** できます。各 buildType の `res.srcDirs` に追加する形で実装
されており、AGP のリソースマージ機構をそのまま使うので、独自タスクや事前コピーは
不要です。

```jsonc
"assetPack": {
    "baseFolder": "${KRKRZ_BASE}/myproject",
    "resOverride": "android-res",
    // 配列で複数のオーバーレイも指定可能 (後ろほど優先):
    // "resOverride": ["base-res", "brand-res"],
    "sources": [ ... ]
}
```

外部フォルダ構造の例 (`<baseFolder>/android-res/` 配下):

```
android-res/
├── mipmap-mdpi/ic_launcher.webp        # ← app/src/main/res の同名を上書き
├── mipmap-hdpi/ic_launcher.webp
├── mipmap-xhdpi/ic_launcher.webp
├── mipmap-xxhdpi/ic_launcher.webp
├── mipmap-xxxhdpi/ic_launcher.webp
├── mipmap-anydpi-v26/ic_launcher.xml   # adaptive icon
├── drawable/ic_launcher_foreground.xml
└── values/strings.xml                  # <string name="app_name">..</string>
                                        # など要素単位で上書き可能
```

仕様:

- `resOverride` の値は `baseFolder` からの相対フォルダ名 (またはその配列)。
- フォルダ構造は **Android `res/` 規約** (`mipmap-*`, `drawable*`, `values*` 等の
  qualifier dir 構造) に従ってください。素の PNG をフラットに置く形には対応しません。
- 全 buildType (debug / release / releaseDebugSigned) に同じオーバーレイが適用されます。
- `values/strings.xml` のような `values` リソースは **要素単位でマージ** されるため、
  `app_name` だけ書いたファイルを置けば他の string はそのまま残ります。
- 配列で指定した場合、**後ろの要素ほど優先** (AGP の `srcDirs` の追加順に従う)。

## パス文字列の `${...}` 展開

`app-config.json` 内のパス文字列は `${...}` 形式で展開できます。

- `${BUILD_SYSTEM_DIR}`: 予約変数。**このリポジトリのルート** (gradle / cmake / cpp/java のソース置き場) の絶対パスに展開されます。Android 専用プラグインなどビルドシステム側のフォルダを指す用途。
- `${PROJECT_DIR}`: 予約変数。**案件フォルダ** (`app-config.json` を含むフォルダ) の絶対パスに展開されます。環境変数 `PROJECT_DIR` 未設定時は `BUILD_SYSTEM_DIR` と同値 (サンプル動作)。
- `${ENV_VAR}`: プロセスの環境変数を参照します。未定義時は **空文字** に展開されます (例: `${KRKRZ_BASE}/foo` で `KRKRZ_BASE` 未設定なら `/foo` になる)。

### 相対パスの解決

`${...}` 展開後に相対パスが残っている場合、**`${PROJECT_DIR}` 基準** で解決されます
(以前は「このリポジトリのルート」基準でしたが、案件フォルダ分離に合わせて変更)。

### 展開が使える設定

| 設定 | 役割 |
|------|------|
| `APP_CONFIG_FILE` (`local.properties` / 環境変数) | 外部 `app-config.json` のパス。指定無しなら `${PROJECT_DIR}/app-config.json` |
| `assetPack.baseFolder` | `sources[].from` の相対起点 |
| `assetPack.sources[].from` | コピー元フォルダ (絶対指定時) |
| `assetPack.resOverride` | res オーバーレイフォルダ。文字列または配列 |
| `cmake.pluginFolders[]` | プラグイン配置ディレクトリ |

> `cmake.plugins` / `cmake.staticPlugins` はパスではなくプラグイン名なので展開対象外です。

### 使用例

```jsonc
{
    "assetPack": {
        "baseFolder": "${PROJECT_DIR}",
        "resOverride": "android-res",
        "sources": [
            { "type": "mirror", "from": "data" },
            { "type": "mirror",
              "from": "${KRKRZ_BASE}/krkrz_dev/src/core/resource",
              "to": "" }
        ]
    },
    "cmake": {
        "pluginFolders": [
            "${BUILD_SYSTEM_DIR}/app/src/main/cpp",
            "${KRKRZ_BASE}/krkrz_dev/src/plugins"
        ]
    }
}
```

## ビルドと実行 (Gradle)

プロジェクトのルートディレクトリで以下のコマンドを実行してください。

### デバッグビルド (APK生成)
```bash
./gradlew assembleDebug
```
生成された APK は `${PROJECT_DIR}/build/android/app/outputs/apk/debug/` に出力されます (サンプル動作時はリポジトリ直下の `build/android/app/outputs/apk/debug/`)。

### デバイスへのインストール (デバッグ)
Android デバイスを接続した状態で実行します。
```bash
./gradlew installDebug
```

### リリースビルド
```bash
./gradlew assembleRelease
```
※ リリースビルドを行うには、以下の署名設定 (Signing Config) が必要です。

## Release (Google Play 用署名)

Google Play にアプリを公開するには、リリース用の署名が必要です。

### 1. キーストアファイルの生成

`keytool` コマンドでキーストアファイル（`.jks`）を生成します。

```bash
keytool -genkey -v -keystore release-key.jks -keyalg RSA -keysize 2048 -validity 10000 -alias my-key-alias
```

| オプション | 説明 |
|-----------|------|
| `-keystore` | 生成するキーストアファイル名 |
| `-keyalg` | 鍵アルゴリズム（RSA推奨） |
| `-keysize` | 鍵サイズ（2048以上推奨） |
| `-validity` | 有効日数（10000日 ≒ 27年） |
| `-alias` | 鍵のエイリアス名 |

コマンド実行時に以下の情報を入力します：
- キーストアのパスワード
- 姓名、組織名、都市、国コード など
- 鍵のパスワード（キーストアと同じでも可）

> **重要**: キーストアファイルとパスワードは紛失しないよう安全に保管してください。Google Play にアップロードしたアプリの更新には同じ鍵が必要です。

### 2. 署名情報の設定

署名情報は `local.properties` に記載します。このファイルは既に `.gitignore` に含まれているため、機密情報が Git にコミットされる心配がありません。

`app/build.gradle` には `local.properties` を読み込んで署名設定に使用するコードが既に含まれています。以下のプロパティを `local.properties` に追記するだけで署名が有効になります。

**local.properties に追記:**
```properties
# Release signing
RELEASE_STORE_FILE=../release-key.jks
RELEASE_STORE_PASSWORD=your_keystore_password
RELEASE_KEY_ALIAS=my-key-alias
RELEASE_KEY_PASSWORD=your_key_password
```

CI/CD 環境など `local.properties` が存在しない場合は、環境変数でも指定できます：
```bash
export RELEASE_STORE_FILE=../release-key.jks
export RELEASE_STORE_PASSWORD=your_keystore_password
export RELEASE_KEY_ALIAS=my-key-alias
export RELEASE_KEY_PASSWORD=your_key_password
```

### 3. .gitignore への追加

キーストアファイルをリポジトリにコミットしないよう `.gitignore` に以下を追加します。  
（`local.properties` は既に `.gitignore` に含まれているため、署名情報は自動的に除外されます）

```gitignore
# Signing keys
*.jks
*.keystore
```

### 4. 署名済みリリースビルドの生成

設定完了後、以下のコマンドで署名済み APK/AAB を生成できます。

```bash
# 署名済み APK
./gradlew assembleRelease

# Google Play 用 App Bundle (推奨)
./gradlew bundleRelease
```

生成物の場所:
- APK: `${PROJECT_DIR}/build/android/app/outputs/apk/release/app-release.apk`
- AAB: `${PROJECT_DIR}/build/android/app/outputs/bundle/release/app-release.aab`

### 5. デバッグ署名でのリリースビルド (release-debug)

リリースビルド (proguard / minify などのリリース設定が適用された状態) を、
**release キーストアを用意せずに debug キーで署名** して動作確認したい場合のための
ビルドタイプ `releaseDebugSigned` を用意しています。

用途:
- リリース構成での挙動・パフォーマンスを手元の端末で確認したい
- まだ release キーストアを発行していない / CI に署名情報を渡せない段階でも、
  リリース構成の APK を生成・インストールしたい
- Play Console 用ではなく、社内配布や動作検証用の APK が欲しい

`app/build.gradle` の `buildTypes.releaseDebugSigned` で release を `initWith` した上で
`signingConfig` を `signingConfigs.debug` に差し替える構成になっています。
`local.properties` の `RELEASE_*` 設定は不要です。

```bash
# Release 構成 + debug 署名で APK を生成
make release-debug

# 端末にインストール
make install-release-debug
```

直接 Gradle を叩く場合:

```bash
./gradlew :app:assembleReleaseDebugSigned
./gradlew :app:installReleaseDebugSigned
```

生成された APK は `${PROJECT_DIR}/build/android/app/outputs/apk/releaseDebugSigned/` に出力されます。

> 注意: debug キーで署名されるため、**Google Play へはアップロードできません**。
> あくまで動作検証専用です。Play 配布用には通常の `assembleRelease` / `bundleRelease`
> (release キーストア署名) を使用してください。
> また、Google Play Games Services を利用する場合は debug キーストアの SHA-1 も
> Play Console 側に登録しておく必要があります (後述「Google Play Games Services プラグイン」参照)。

### 6. 署名の検証

生成された APK/AAB が正しく署名されているか確認できます。

```bash
# APK の署名確認
apksigner verify --verbose "${PROJECT_DIR:-.}/build/android/app/outputs/apk/release/app-release.apk"

# AAB の署名確認 (bundletool 使用)
jarsigner -verify -verbose "${PROJECT_DIR:-.}/build/android/app/outputs/bundle/release/app-release.aab"
```

### Google Play App Signing について

Google Play Console では「Play App Signing」を利用することを推奨しています。これを使用すると：

- Google がアップロード鍵とは別の署名鍵でアプリに再署名
- アップロード鍵を紛失しても Google に依頼して復旧可能
- より安全な鍵管理が可能

初回アップロード時に Play App Signing への登録を選択できます。

### CI/CD での署名

GitHub Actions などの CI/CD 環境では、秘密情報を Secrets として登録し、環境変数経由で渡します。

**GitHub Actions の例:**
```yaml
env:
  RELEASE_STORE_PASSWORD: ${{ secrets.RELEASE_STORE_PASSWORD }}
  RELEASE_KEY_ALIAS: ${{ secrets.RELEASE_KEY_ALIAS }}
  RELEASE_KEY_PASSWORD: ${{ secrets.RELEASE_KEY_PASSWORD }}
```

キーストアファイルは Base64 エンコードして Secret に保存し、ビルド時にデコードします。

```yaml
- name: Decode Keystore
  run: echo "${{ secrets.RELEASE_KEYSTORE_BASE64 }}" | base64 -d > release-key.jks
```

## 自前アプリへのカスタマイズ

独自のアプリとしてリリースする場合、以下のファイルを修正してください。

### 1. アプリID・バージョン情報の変更
プロジェクトルートの `app-config.json` ファイルを編集してください。
(または下記「外部 app-config.json への切り替え」で別ファイルを参照させることもできます)

```json
{
    "applicationId": "com.example.mygame",
    "versionCode": 2,
    "versionName": "1.1",
    "compileSdk": 36,
    "minSdk": 29,
    "ndkVersion": "28.2.13676358"
}
```

* `applicationId`: アプリの一意な識別子（パッケージ名）
* `versionCode`: 内部バージョン番号（整数、リリースごとに増やす）
* `versionName`: ユーザーに表示されるバージョン文字列
* `compileSdk`: コンパイルに利用するSDKバージョン
* `minSdk`: 最小動作対象SDKバージョン
* `ndkVersion`: 利用する NDK のバージョン

※ `app/build.gradle` はこの JSON ファイルを自動的に読み込みます。

### 1.0.0. APK / AAB のファイル名 (apk)

`apk` セクションでビルド成果物のファイル名を制御できます。

```jsonc
"apk": {
    "baseName": "myapp"   // 既定: "app"
}
```

挙動:

- **ファイル名**: `<baseName>-<versionName>-<variant>.apk` 形式に変更
  (例: `myapp-1.0-debug.apk`、`myapp-1.0-release.apk`、
  `myapp-1.0-releaseDebugSigned.apk`)。AGP 標準の出力先
  (`${PROJECT_DIR}/build/android/app/outputs/apk/<variant>/`) のファイル名が
  そのまま変わります。
- **AAB の出力先**は `app-<variant>.aab` のまま (AGP 制約)。

仕様:

- `apk` セクション省略時は AGP 既定 (`app-<variant>.apk` / `app-<variant>.aab`)。
- 出力先を別フォルダに集めたい場合は、案件側で `PROJECT_DIR` を切り替えるか、
  シェルスクリプト等で `${PROJECT_DIR}/build/android/app/outputs/apk/...` から
  コピー / リンクしてください (以前あった `apk.outputDir` 機能は `PROJECT_DIR`
  方式に統合して廃止しました)。

### 1.0.1. 案件フォルダの指定 (PROJECT_DIR / APP_CONFIG_FILE)

**通常の運用は `PROJECT_DIR` 環境変数を使ってください** (前述
「ビルドシステムフォルダと案件フォルダの分離」参照)。`PROJECT_DIR` を指定すると
案件側の `app-config.json`、リソース、ビルド出力先すべてがその配下に揃います。

```bash
export PROJECT_DIR=/path/to/myproject
./gradlew assembleDebug
```

`PROJECT_DIR` 未指定時は `BUILD_SYSTEM_DIR` (このリポジトリ) にフォールバックします。

**より細かい上書き: `APP_CONFIG_FILE`** — `app-config.json` の位置だけを別の場所に
向けたい場合 (例: PROJECT_DIR は案件フォルダのままで、config だけバリアント切替)
に使えます。

優先順位:

1. `local.properties` の `APP_CONFIG_FILE=...`
2. 環境変数 `APP_CONFIG_FILE=...`
3. 未指定時は `${PROJECT_DIR}/app-config.json`

```properties
# local.properties
APP_CONFIG_FILE=C:/work/myproject/app-config.json
# 環境変数展開も可
# APP_CONFIG_FILE=${KRKRZ_BASE}/myproject/app-config.json
```

仕様:

- パス文字列は `${BUILD_SYSTEM_DIR}` / `${PROJECT_DIR}` / `${ENV_VAR}` の展開に対応
- 絶対パス、または相対パス (相対の場合は **`PROJECT_DIR` 基準** で解決)
- 指定があってファイルが存在しない場合は **ビルドが即座にエラー停止** (誤指定の早期発見)

注意:

- **Makefile 側の MSYS2 制約**: MSYS2 の make は外部シェルの環境変数を取りこぼす
  ことがあるため、`make` 経由で `PROJECT_DIR` を渡すときは **make 引数として** 
  指定するのが確実です:
  ```bash
  make build PROJECT_DIR=/path/to/myproject
  ```
  `./gradlew` 直接呼び出しは環境変数 (`export PROJECT_DIR=...`) で問題ありません。

### 1.1. AndroidManifest の値を外部から差し替える (manifestPlaceholders)

`AndroidManifest.xml` 内の `${KEY}` 形式のプレースホルダを、
`app-config.json` の `manifestPlaceholders` で置換できます。
画面向き、API キー、Deep Link のホスト名など、Manifest 直書きの単一値を
**Manifest を直接編集せずに** アプリごとに切り替える用途に使えます。

```json
{
    "applicationId": "com.example.mygame",
    "manifestPlaceholders": {
        "screenOrientation": "landscape",
        "deepLinkHost": "example.com"
    }
}
```

`AndroidManifest.xml` 側でプレースホルダを書いておくと、ビルド時に置換されます:

```xml
<activity
    android:name="..."
    android:screenOrientation="${screenOrientation}" >
    <intent-filter>
        <data android:scheme="https" android:host="${deepLinkHost}" />
    </intent-filter>
</activity>
```

`manifestPlaceholders` を省略した場合は何も置換されないので、
プレースホルダを使わない既定の Manifest はそのままビルドできます。

### 1.2. CMake プラグイン構成 (cmake)

KrkrZ プラグインの取り込み構成 (旧 `myapp.cmake` の内容) は `app-config.json` の
`cmake` セクションで管理します。`preBuild` および CMake configure 前に
`generateMyappCmake` タスクがこのセクションを読み、
`${PROJECT_DIR}/build/android/app/generated/cmake/myapp.cmake` を自動生成し、
CMake (`app/src/main/cpp/CMakeLists.txt`) から
`-DMYAPP_CMAKE_FILE=<absolute path>` 経由で `include()` されます。

```json
"cmake": {
    "pluginFolders": [
        "${BUILD_SYSTEM_DIR}/app/src/main/cpp",
        "${KRKRZ_BASE}/wamsoft_work",
        "${KRKRZ_BASE}/krkrtemplate/plugins"
    ],
    "plugins": [
        "csvParser",
        "playgames",
        "json",
        "saveStruct"
    ],
    "staticPlugins": [
        "playgames"
    ]
}
```

| キー | 内容 | 生成される CMake 変数 |
|------|------|---------------------|
| `pluginFolders` | プラグインを探すフォルダ。相対パス (`PROJECT_DIR` 基準) / 絶対パス / `${BUILD_SYSTEM_DIR}` / `${PROJECT_DIR}` / `${ENV_VAR}` 展開が利用可能 | `TVP_PLUGIN_FOLDERS` |
| `plugins` | リンクするプラグイン名の列挙 | `TVP_PLUGINS` |
| `staticPlugins` | `libkrkrz.so` に静的に組み込むプラグイン名 | `TVP_PLUGINS_STATIC` |

仕様:

- **生成先は `${PROJECT_DIR}/build/android/app/generated/cmake/myapp.cmake`** で、
  `build/android/` 配下なので既存の `.gitignore` (`build/`) で自動的にコミット
  対象外になります。直接編集しても次回ビルドで上書きされるので、設定変更は必ず
  `app-config.json` 側で行ってください。
- 各パスは gradle 側で **絶対パスに解決** された状態で書き出されます。
  CMake 側で `${CMAKE_CURRENT_LIST_DIR}` 等を意識する必要はありません。
- このリポジトリ内 (`app/src/main/cpp` 等) のプラグインフォルダを指す場合は
  `${BUILD_SYSTEM_DIR}/app/src/main/cpp` を明示してください。相対パスは
  `${PROJECT_DIR}` 基準で解決されるので、案件フォルダ側に存在しないと無視されます。
- `cmake` セクション自体を省略した場合、`generateMyappCmake` は何もしません。

### 2. アプリ名の変更
`app/src/main/res/values/strings.xml` を開き、`app_name` を変更してください。

```xml
<resources>
    <string name="app_name">あなたのアプリ名</string>
</resources>
```

## アプリアイコンの変更

アプリアイコンは `app/src/main/res/` 以下の `mipmap` フォルダに格納されています。
`AndroidManifest.xml` では以下のように定義されています。

* **通常アイコン**: `@mipmap/ic_launcher`
* **丸型アイコン**: `@mipmap/ic_launcher_round` (Android 7.1以降で使用)

### 手順
以下の各フォルダにある `ic_launcher.png` と `ic_launcher_round.png` を、自前のアイコン画像に差し替えてください。

* `mipmap-mdpi/` (48x48)
* `mipmap-hdpi/` (72x72)
* `mipmap-xhdpi/` (96x96)
* `mipmap-xxhdpi/` (144x144)
* `mipmap-xxxhdpi/` (192x192)

※ Android Studio の "Image Asset Studio" 機能を使うと、これらのアイコンを一括で生成・配置できるため便利です。

## 独自の Activity を作成する

デフォルトの `KrkrzActivity` を継承して、独自の初期化処理や機能追加を行うことができます。

### 1. Java/Kotlin ソースディレクトリの作成
`app/src/main/java/` ディレクトリを作成し、パッケージ名に合わせたフォルダ階層を作成します。
例: パッケージ名が `com.example.mygame` の場合
`app/src/main/java/com/example/mygame/`

### 2. Activity クラスの作成
`jp.wamsoft.krkrz.KrkrzActivity` を継承したクラスを作成します。

**例: MainActivity.java**
```java
package com.example.mygame;

import android.os.Bundle;
import jp.wamsoft.krkrz.KrkrzActivity;

public class MainActivity extends KrkrzActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // ここに独自の初期化処理を記述
    }
}
```

### 3. AndroidManifest.xml の修正
`app/src/main/AndroidManifest.xml` を開き、`<activity>` タグの `android:name` 属性を作成したクラス名に変更します。

```xml
<activity
    android:name="com.example.mygame.MainActivity"
    android:exported="true"
    android:configChanges="layoutDirection|locale|orientation|..."
    >
    <!-- ... -->
</activity>
```

## JNI (C++) で関数を追加する

吉里吉里Z の機能に加え、ネイティブコード (C/C++) で
独自の処理を実装する場合の手順です。

### 1. C++ ソースの配置

`app/src/main/cpp/` 以下に C++ ソースファイル（例: `native-lib.cpp`）を配置します。

### 2. CMakeLists.txt に項目追加

`app/src/main/cpp/` の `CMakeLists.txt` を編集して、ビルド設定を記述します。

**例: CMakeLists.txt**

```cmake

add_library(my-native-lib SHARED
        native-lib.cpp)

find_library(
        log-lib
        log)

target_link_libraries(
        my-native-lib
        ${log-lib})
```

### 4. ライブラリのロードとメソッド定義
継承した Activity (例: `MainActivity.java`) でライブラリをロードし、ネイティブメソッドを宣言します。

```java
public class MainActivity extends KrkrzActivity {
    static {
        System.loadLibrary("my-native-lib");
    }

    // ネイティブメソッドの宣言
    public native String stringFromJNI();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // ...
    }
}
```

## ビルドと実行 (Makefile)

プロジェクトルートの [Makefile](Makefile) から `gradlew` を呼び出して、ビルド・インストール・起動・デバッグ・テストなどを行えます。

前提
- GNU Make が利用できるシェル環境 (Git Bash / MSYS2 / WSL 等)
- Android SDK (`adb`) は `local.properties` の `sdk.dir` から自動検出。見つからない場合は `ADB` で明示指定可能

主なターゲット
- build/assemble: `assemble<BUILD_TYPE>` の実行 (既定: Debug)
- bundle: `bundle<BUILD_TYPE>` の生成
- release-debug / install-release-debug: Release 構成を debug 鍵で署名した APK の生成 / インストール (release キーストア不要、Play 配布不可)
- install/reinstall/uninstall: 端末へのインストール/再インストール/アンインストール
- run: ランチャーアクティビティを起動
- debug: デバッガ待機で起動 (`am start -D`)、後からアタッチ
- stop: アプリの強制停止
- logcat: `adb logcat` のフォロー
- clean: gradle clean (build/ 配下クリーン)
- clean-cmake: CMake 生成物 (`.cxx/`、`build/intermediates/cxx`、`build/generated/cmake`) のみ削除。プラグイン構成変更後の再構成に
- distclean: clean + clean-cmake。新規チェックアウト相当の状態に戻す (vcpkg 再 install、CMake 完全再 configure)
- test/connected: ユニットテスト/計測テスト
- devices: 接続デバイス一覧
- vars: 変数の解決結果を表示

上書き可能な変数
- MODULE: 対象モジュール名 (既定: `app`)
- BUILD_TYPE: `Debug` または `Release` (既定: `Debug`)
- APP_ID: アプリID。`app/build.gradle` の `applicationId` → `AndroidManifest.xml` の `package` の順で自動検出。未検出時は明示指定
- ADB: `adb` のパス。未指定時は `sdk.dir` から推測、無ければ `adb`
- GRADLEW: `gradlew` の呼び出し (既定: `./gradlew`)

使用例
```bash
# ビルド (Debug 既定)
make build

# Releaseビルド
make build BUILD_TYPE=Release

# インストールと起動
make run

# デバッガ待機で起動し、IDEからアタッチ
make debug

# 再インストール
make reinstall

# アンインストール
make uninstall

# ログを表示 (終了は Ctrl+C)
make logcat

# ユニットテスト / 計測テスト
make test
make connected

# 変数の確認
make vars

# 例: アプリIDを明示して起動
make run APP_ID=com.example.mygame

# 例: 別モジュールを対象
make install MODULE=myfeature

# 例: adb を明示指定 (Windows の SDK 既定パス例)
make devices ADB="/c/Android/sdk/platform-tools/adb"
```

メモ
- Windows の場合は Git Bash / MSYS2 などの GNU Make 環境での実行を推奨します。`mingw32-make` でも可。

## Google Play Games Services プラグイン

`app/src/main/cpp/playgames/` および `app/src/main/java/jp/wamsoft/krkrz/playgames/` に
Play Games Services 連携プラグインが組み込まれています。
TJS から `PlayGames` クラスでアクセスできます (API 詳細は
`app/src/main/cpp/playgames/manual.tjs` 参照)。

### 1. Play Console セットアップ

1. [Google Play Console](https://play.google.com/console/) で対象アプリの「ゲームサービス」を有効化
2. ゲームサービス用の **App ID** (10 桁前後の数値) を取得
3. 必要に応じて実績 / セーブゲーム機能を有効化

### 2. App ID をアプリに設定

`app/src/main/res/values/strings.xml` の `games_app_id` を取得した ID に書き換えます。

```xml
<string name="games_app_id" translatable="false">1234567890</string>
```

未設定 (`0000000000` のまま) でもビルドは通りますが、TJS から `PlayGames` API を呼ぶと
SDK 初期化失敗のエラーが返ります。

### 3. SHA-1 フィンガープリント登録

Play Console の「OAuth 2.0 クライアント」に、署名鍵 (`release-key.jks`) の SHA-1 を
登録する必要があります。

```bash
keytool -list -v -keystore release-key.jks -alias my-key-alias
```

デバッグビルドで動作確認する場合は、Android Studio が生成する debug キーストア
(`~/.android/debug.keystore`、パスワード `android`) の SHA-1 も登録します。

### 4. テスター登録

Play Games Services はリリース前 (テスト中) は明示的にテスターとして登録した
Google アカウントでのみ動作します。Play Console > ゲームサービス > テスター で
追加してください。

### 5. プラグインの有効/無効切り替え

プラグイン C++ は `myapp.cmake` の `TVP_PLUGINS` / `TVP_PLUGINS_STATIC` から
`playgames` を外せばビルド対象外になります (Java 側のソースは残ったままでも
SDK が初期化されないだけなので無害)。Java 依存を削るには `app/build.gradle` の
`com.google.android.gms:play-services-games-v2` 行も削除します。

### 詳細資料

- [`app/src/main/cpp/playgames/SETUP.md`](app/src/main/cpp/playgames/SETUP.md):
  Play Console セットアップの詳細手順、テスト用チェックリスト、
  トラブルシューティング、拡張ロードマップ
- [`app/src/main/cpp/playgames/manual.tjs`](app/src/main/cpp/playgames/manual.tjs):
  TJS 側 API リファレンス
- [`docs/android-jni-plugin.md`](docs/android-jni-plugin.md):
  Android 用 JNI プラグインを **追加で** 作るときの汎用テンプレート手順
