# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## プロジェクト概要

吉里吉里Z (Krkrz) を Android 上で動作させるためのプロジェクトテンプレート。
SDL3 上で動く KrkrZ エンジンを JNI から呼び出し、`assets/data/startup.tjs` から TJS スクリプトを起動する構成。

このリポジトリは「ガワ」のみで、エンジン本体・プラグインのソースは **外部ディレクトリから参照** する。リポジトリ内だけを読んでもエンジン側の実装は見えない点に注意。

## 必須の事前セットアップ

ビルドに先立って以下が必要。これが揃っていないと CMake 段階で必ず失敗する。

- `VCPKG_ROOT` 環境変数: vcpkg のルート。`app/build.gradle` が `${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake` をツールチェインとして渡す。
- `KRKRZ_BASE` 環境変数: `krkrz_dev` フォルダがあるディレクトリ。`app/src/main/cpp/CMakeLists.txt` が `${KRKRZ_BASE}/krkrz_dev/src/core` (本体) と `${KRKRZ_BASE}/krkrz_dev/src/plugins` (プラグイン群) を取り込む。さらに `app/build.gradle` が `${KRKRZ_BASE}/krkrz_dev/src/core/resource` を追加 assets ソースとして読み込む。
- `app/libs/SDL3-*.aar`: SDL3 の AAR を配置する。`./install-sdl-aar.sh` で固定バージョンをダウンロードして配置できる (現在 3.4.0)。`app/build.gradle` の `fileTree(dir: 'libs', include: ['*.aar'])` で自動 implementation される。
- Android SDK は `local.properties` の `sdk.dir` から検出される。

## よく使うコマンド

ルートの `Makefile` が `./gradlew` の薄いラッパー。Git Bash / MSYS2 環境を想定。

```bash
make build                    # assembleDebug
make build BUILD_TYPE=Release # assembleRelease
make install                  # installDebug
make run                      # install + monkey で LAUNCHER 起動
make debug                    # am start -D でデバッガ待機起動
make reinstall                # uninstall してから再 install
make logcat                   # adb logcat フォロー
make clean                    # gradlew clean
make test                     # testDebugUnitTest
make connected                # connectedDebugAndroidTest
make vars                     # 解決された MODULE/BUILD_TYPE/ADB/APP_ID を表示
```

直接 Gradle を叩く場合 (Makefile が使えない環境):

```bash
./gradlew assembleDebug
./gradlew installDebug
./gradlew bundleRelease       # Google Play 用 AAB
./gradlew :app:testDebugUnitTest
```

`APP_ID` は `app-config.json` の `applicationId` から自動検出される。検出に失敗する場合は `make run APP_ID=...` で明示する。

## アーキテクチャの要点

### Java 層から native 層への接続

エントリは `app/src/main/java/jp/wamsoft/krkrz/KrkrzActivity.java` のみ。これは SDL3 の `org.libsdl.app.SDLActivity` を継承し、

1. `getLibraries()` で `["SDL3", "krkrz"]` を返す → 起動時に `libSDL3.so` → `libkrkrz.so` の順に `System.loadLibrary` される。
2. `onCreate` で `setAssetManager(getAssets())` を JNI 呼び出し → ネイティブ側 (`andres.cpp`、`KRKRZ_BASE` 配下) の `Java_jp_wamsoft_krkrz_KrkrzActivity_setAssetManager` で AssetManager を保持し、`assets/` 内の TJS スクリプト・データを読めるようにする。

独自 Activity を作る場合もこのライブラリ名と JNI シグネチャ (`jp.wamsoft.krkrz.KrkrzActivity` パッケージ・クラス) を変更すると native 側のシンボルと食い違うので、**継承で拡張するのが基本**。継承する場合は `AndroidManifest.xml` の `android:name` を新クラスに差し替える。

### Native ビルドの組み立て

`app/src/main/cpp/CMakeLists.txt` がエンジン本体・プラグインを `add_subdirectory` で取り込む構成。重要なポイント:

- ABI → vcpkg triplet マッピングを **手動で行っている** (`arm64-v8a` → `arm64-android` 等)。vcpkg 側の自動判定が効かないため。新しい ABI を追加するときはこのマッピングも追加する必要がある。
- `app/build.gradle` の `abiFilters` は `'arm64-v8a', 'x86_64'` に固定。armeabi-v7a / x86 はビルド対象外。
- KrkrZ プラットフォーム定義 (`KRKRZ_DEFINES_PLATFORM`) が CMakeLists.txt 内で明示されている: パス正規化無効、ファイル検索ケース無視、ローカルファイルはケース強制、PROVINCE/MASK 自動ロード無効、保存時 autopath キャッシュ非クリア、miniaudio ランタイムリンク無効。これらは Android 上の挙動を成立させる前提なので変えるとレグレッションになりやすい。
- ネイティブ依存は vcpkg manifest (`app/src/main/cpp/vcpkg.json`)。SDL3 だけは prefab 経由で `find_package(SDL3 REQUIRED CONFIG)` から解決される (AAR 内の prefab に依存、`buildFeatures { prefab = true }` で有効化)。
- 最終的なターゲット名 `krkrz` (= `libkrkrz.so`) が KrkrzActivity からロードされる。追加ネイティブライブラリを足す場合は CMakeLists.txt の末尾に `add_library` し、Java 側で `System.loadLibrary` も追加する。

### プラグインの組み込み

ルート直下 `myapp.cmake` で取り込み対象を追加する (CMakeLists.txt から include される):

- `TVP_PLUGIN_FOLDERS`: `krkrz_dev/src/plugins` 以外のプラグイン配置ディレクトリを追加 (krkrz_dev と同じ構造を想定)
- `TVP_PLUGINS`: 個別プラグイン名を列挙
- `TVP_PLUGINS_STATIC`: `libkrkrz.so` に静的に組み込むプラグイン名を列挙

**Android 専用プラグインの配置ルール**: Java 連携が必要なプラグインは
`app/src/main/cpp/<name>/` (C++) と `app/src/main/java/jp/wamsoft/krkrz/<name>/` (Java) に置く。
`myapp.cmake` の `TVP_PLUGIN_FOLDERS` に `${CMAKE_CURRENT_LIST_DIR}/app/src/main/cpp` を
追加した上で、`TVP_PLUGINS` と `TVP_PLUGINS_STATIC` の両方に登録する。
Java 依存は `app/build.gradle` に直接書く (krkrz_dev 側ではなくアプリ側で抱える方針)。

例: `playgames` プラグイン (Google Play Games Services)
- C++: `app/src/main/cpp/playgames/` (Main.cpp + JniBridge.cpp + サブシステムごと)
- Java: `app/src/main/java/jp/wamsoft/krkrz/playgames/` (PlayGamesBridge ほか)
- `app/build.gradle` に `com.google.android.gms:play-services-games-v2` 依存追加
- `app/src/main/res/values/strings.xml` の `games_app_id` に Play Console の App ID を設定
- `AndroidManifest.xml` に `com.google.android.gms.games.APP_ID` meta-data 追加
- 運用手順 (Play Console セットアップ・テスト・拡張): `app/src/main/cpp/playgames/SETUP.md`
- 同種のプラグインを新規追加するときの汎用手順: `docs/android-jni-plugin.md`

`KrkrzActivity.onCreate` でプラグインの Java 側初期化フックを呼ぶ
(例: `PlayGamesBridge.initialize(this)`)。
ネイティブ側コールバックは `tTVPContinuousEventCallbackIntf` 上で
スレッドセーフキューを drain する設計 (`JniBridge.cpp` の `drainCallbacks`)。
JNI ネイティブメソッドの実装は `JNIEXPORT` で書いておけば
`krkrz_plugin` マクロが付ける `--whole-archive` リンカフラグで `libkrkrz.so` から露出される。

### ビルド設定の単一ソース

バージョン・SDK・パッケージ名は **`app-config.json` が単一ソース**。`app/build.gradle` が `JsonSlurper` でパースして `applicationId` / `versionCode` / `versionName` / `compileSdk` / `targetSdk` / `minSdk` / `ndkVersion` に流し込む。Makefile も `APP_ID` を同 JSON から正規表現で抜き出す。`build.gradle` を直接書き換えず、JSON を編集するのが正規ルート。

### 署名

Release 署名は `local.properties` の `RELEASE_STORE_FILE` / `RELEASE_STORE_PASSWORD` / `RELEASE_KEY_ALIAS` / `RELEASE_KEY_PASSWORD` から読む (なければ同名の環境変数にフォールバック)。`local.properties` は `.gitignore` 済みなのでそこに置く。CI では環境変数経由で渡す。

## 編集時の注意

- `app/build.gradle` の `applicationId` 等を直接編集しない。`app-config.json` を更新する。
- ABI を増やす場合は CMakeLists.txt の triplet マッピングと `app/build.gradle` の `abiFilters` の両方を揃える。
- KrkrZ 本体・プラグインの実装に手を入れる必要がある場合、対象は `${KRKRZ_BASE}/krkrz_dev/` 配下であってこのリポジトリではない。リポジトリ内を grep しても見つからないのは仕様。
- `assets/data/startup.tjs` がアプリ起動時のエントリ。`app/src/main/assets/data/` は `.gitignore` 対象 (リポジトリにコミットしないテンプレート構成)。
