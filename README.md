# krkrz_android — 吉里吉里Z Android 版

日本語 | [English](README_en.md)

吉里吉里Z (krkrz) を Android で動かすための外枠リポジトリです。Gradle + NDK (CMake) でビルドし、
システムまわりは SDL3 を使います。

- エンジン本体とプラグインのソースは [krkrz_dev](https://github.com/wamsoft/krkrz_dev) を `KRKRZ_BASE`
  経由で参照します。このリポジトリには Android 固有のビルド定義とパッケージングだけを置きます
- タイトルごとの設定 (`app-config.json`) と資材は別のフォルダ (プロジェクトフォルダ) に置けます。
  このリポジトリだけでも、同梱のサンプルとしてビルドできます
- `cmake` (プラグイン構成) と `assetPack` (資材の取り込み) は krkrz_ios / krkrz_linux / krkrz_gdk /
  krkrz_web と共通の書式です

## 必要なもの

- Android SDK と NDK (NDK のバージョンは `app-config.json` の `ndkVersion`)
- JDK 17 以降 (Gradle 9 の実行に必要)
- [vcpkg](https://github.com/microsoft/vcpkg) — 環境変数 `VCPKG_ROOT` に導入先を指定
- [krkrz_dev](https://github.com/wamsoft/krkrz_dev) — このリポジトリと同じ場所に clone し、環境変数
  `KRKRZ_BASE` にその親フォルダを指定
- GNU Make が使えるシェル (Git Bash / MSYS2 / WSL など。Makefile を使う場合)

## セットアップ

1. krkrz_dev とこのリポジトリを同じフォルダに clone します

   ```
   <KRKRZ_BASE>/
   ├── krkrz_dev/        ← git clone --recursive https://github.com/wamsoft/krkrz_dev.git
   └── krkrz_android/    ← このリポジトリ
   ```

2. 環境変数 `VCPKG_ROOT` と `KRKRZ_BASE` を設定します
3. SDL3 の `.aar` を `app/libs/` に置きます。`install-sdl-aar.sh` を実行すると、決まった版の SDL3 を
   取ってきて置きます (`app/libs/` の `.aar` は全て自動で読まれます)
4. `local.properties` に Android SDK の場所 (`sdk.dir`) を書きます (Android Studio で開くと作られます)

エンジン本体は `${KRKRZ_BASE}/krkrz_dev/src/core`、プラグインは `${KRKRZ_BASE}/krkrz_dev/src/plugins` を
参照します (`app/src/main/cpp/CMakeLists.txt`)。

## ビルドと実行

```bash
make build      # このリポジトリ同梱のサンプルをビルド (Debug)
make run        # 端末に入れて起動
make logcat     # ログを表示

make build PROJECT_DIR=/path/to/mygame   # 自分のプロジェクトフォルダをビルド
```

APK は `${PROJECT_DIR}/build/android/app/outputs/apk/<variant>/` に出ます。主なターゲットは次のとおりです。
全ターゲット・変数と Gradle を直接呼ぶ方法は [docs/android-build.md](docs/android-build.md) にあります。

| やること | コマンド |
|---|---|
| ビルド (Debug / Release) | `make build` / `make build BUILD_TYPE=Release` |
| Google Play 用の AAB | `make bundle BUILD_TYPE=Release` |
| 入れて起動 / デバッガ待ちで起動 | `make run` / `make debug` |
| リリース構成を debug の鍵で署名 | `make release-debug` |
| CMake の生成物を消す (プラグイン構成やプロジェクトを変えたあと) | `make clean-cmake` |

## プロジェクトフォルダ (PROJECT_DIR)

このリポジトリは共通のビルドシステムで、ビルドするタイトルの設定と資材は別のフォルダに置けます。

| 変数 | 意味 |
|---|---|
| `BUILD_SYSTEM_DIR` | このリポジトリのルート。Makefile / settings.gradle が自動で決めます |
| `PROJECT_DIR` | プロジェクトフォルダ。`${PROJECT_DIR}/app-config.json` を読み、`${PROJECT_DIR}/build/android/` に出力します。未指定ならこのリポジトリ自身 (サンプル) |
| `KRKRZ_BASE` | krkrz_dev の親フォルダ |

- `app-config.json` の相対パスは `PROJECT_DIR` 基準です。このリポジトリの中を指すときは `${BUILD_SYSTEM_DIR}/...` と書きます
- `local.properties` (SDK の場所・署名情報) はビルド環境の設定なので、このリポジトリ直下のものだけを読みます
- プロジェクトを切り替えたら、一度 `make clean-cmake` してください (`app/.cxx/` に前の CMake のキャッシュが残るため)

## app-config.json

アプリ ID・バージョン・SDK・プラグイン・資材を指定します。リポジトリ直下のファイルがサンプル兼ひな形です。
各項目の説明は [docs/android-config.md](docs/android-config.md) にあります。

| キー | 内容 |
|---|---|
| `applicationId` / `versionCode` / `versionName` | アプリ ID とバージョン |
| `compileSdk` / `targetSdk` / `minSdk` / `ndkVersion` | SDK と NDK の版 |
| `apk.baseName` | APK のファイル名 |
| `manifestPlaceholders` | `AndroidManifest.xml` の `${KEY}` の差し替え |
| `cmake` | 組み込むプラグイン (外枠共通の書式) |
| `assetPack` | 資材の取り込み (外枠共通の書式)。`resOverride` で res (アイコン・アプリ名) を上書き |
| `padPacks` | Play Asset Delivery で配信する大きなデータ |

`cmake` / `assetPack` / パスの `${...}` 展開の書式は、公開サイトの
[外枠リポジトリの共通設定](https://wamsoft.github.io/krkrz_dev/topics/core/platform_config/) を参照してください。

## リリースと Google Play

| やること | 説明 |
|---|---|
| リリース用の鍵で署名する | [docs/android-signing.md](docs/android-signing.md) |
| 大きなデータを Play Asset Delivery で配信する (`padPacks`) | [docs/android-play-asset-delivery.md](docs/android-play-asset-delivery.md) |
| Google Play Games (サインイン・実績・クラウドセーブ) を使う | [docs/android-play-games.md](docs/android-play-games.md) |
| 読み上げ (TalkBack) 対応 (`"a11y"` プラグイン、既定で有効) | [docs/android-accessibility.md](docs/android-accessibility.md) |
| Play Console へのアップロード・公開を `make` から行う | [docs/android-play-publishing.md](docs/android-play-publishing.md) |

Play Games は `app-config.json` の `cmake.plugins` に `"playgames"` を入れるかどうかだけで有効 / 無効が
切り替わります (無効なら依存ライブラリも Manifest の項目も入りません)。

## 自分のアプリにする

アプリ名・アイコン (アダプティブアイコンを含む)・独自の Activity・JNI の関数の足し方は
[docs/android-customize.md](docs/android-customize.md) にあります。

## 開発用の機能

- **REPL** — adb 経由の TJS 対話シェル (`make repl`。Debug ビルドのみ)。[docs/android-repl.md](docs/android-repl.md)
- **adb の操作** — 端末内のファイルの確認・削除、アプリの状態。[docs/android-adb-tips.md](docs/android-adb-tips.md)

## ドキュメント

| ドキュメント | 内容 |
|---|---|
| [docs/android-build.md](docs/android-build.md) | Makefile の全ターゲットと変数、Gradle を直接呼ぶ方法 |
| [docs/android-config.md](docs/android-config.md) | `app-config.json` の各項目、プロジェクトフォルダの指定 |
| [docs/android-signing.md](docs/android-signing.md) | リリース用の署名、debug の鍵で署名したリリースビルド、CI での署名 |
| [docs/android-customize.md](docs/android-customize.md) | アプリ名・アイコン・独自の Activity・JNI |
| [docs/android-play-asset-delivery.md](docs/android-play-asset-delivery.md) | Play Asset Delivery (大きなデータの配信) |
| [docs/android-pad-fast-iteration.md](docs/android-pad-fast-iteration.md) | `make pad-fast` の仕組み |
| [docs/android-play-games.md](docs/android-play-games.md) | Google Play Games Services |
| [docs/android-accessibility.md](docs/android-accessibility.md) | 読み上げ (TalkBack) 対応 |
| [docs/android-play-publishing.md](docs/android-play-publishing.md) | Google Play Console の公開の自動化 |
| [docs/android-jni-plugin.md](docs/android-jni-plugin.md) | JNI プラグインの作り方 |
| [docs/android-repl.md](docs/android-repl.md) | adb 経由の REPL |
| [docs/android-adb-tips.md](docs/android-adb-tips.md) | adb の操作 |

吉里吉里Z 本体のドキュメントは [公開サイト](https://wamsoft.github.io/krkrz_dev/) にあります。

## ライセンス

このリポジトリ (ビルドスクリプト・Android 用のソース (Java / C++)・ツール・テンプレート) は [MIT License](LICENSE) です。

ビルド時に参照・取得する以下のものは、それぞれのライセンスに従います。このリポジトリのライセンスは及びません。

- 吉里吉里Z 本体とプラグイン (krkrz_dev 配下の各 submodule)
- SDL3 (zlib License) と、vcpkg で取得する依存ライブラリ
- Android SDK / NDK と、Gradle で取得するライブラリ (Google Play のライブラリなど)

配布物に必要なライセンスの表記は、使ったエンジン・プラグイン・ライブラリに合わせてタイトル側で同梱してください。
