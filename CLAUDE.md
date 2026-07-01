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

### ビルドシステムと案件フォルダの分離

このリポジトリは「共通のビルドシステム」、ビルド対象の案件 (app-config.json + リソース) は別フォルダ、という構成を取れる。

- `BUILD_SYSTEM_DIR`: このリポジトリのルート (gradle/cmake/cpp/java のソースが置いてある場所)。常に固定で、Makefile / settings.gradle が自動で解決する。
- `PROJECT_DIR` (環境変数): 案件フォルダ。`${PROJECT_DIR}/app-config.json` が参照され、`${PROJECT_DIR}/build/android/` 以下にビルド出力 (`app/` と `root/`) が掃き出される。**未設定の場合 `BUILD_SYSTEM_DIR` にフォールバック** するので、このリポジトリ単体で「サンプル」としてビルドできる。
- `app-config.json` と `*.gradle` 内の `${VAR}` 展開で `${BUILD_SYSTEM_DIR}` と `${PROJECT_DIR}` の両方が使える。相対パスは `${PROJECT_DIR}` 基準で解決される。ビルドシステム側を指したい場合 (例: `app/src/main/cpp` の Android 専用プラグインフォルダ) は `${BUILD_SYSTEM_DIR}/...` を明示する。
- `local.properties` (sdk.dir / 署名情報) は `BUILD_SYSTEM_DIR` 直下のみ参照。案件横断のビルド環境設定なので案件側に置く必要はない。
- 案件を切り替えたら一度 `make clean-cmake` を推奨。`.cxx/` だけは AGP の仕様上 `${BUILD_SYSTEM_DIR}/app/.cxx/` に固定で生成されるため、前案件の CMake/vcpkg 状態が残る可能性がある。

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

例: `androidconfig` プラグイン (機能設定画面 ↔ 吉里吉里本体コンフィグ連携)
- C++: `app/src/main/cpp/androidconfig/` (Main.cpp + ConfigBridge.cpp、static plugin)
- Java: `app/src/main/java/jp/wamsoft/krkrz/config/KrkrzConfig.java` (非同期取得 + UI スレッド配送)、`ui/SettingsActivity.java` が設定行を動的生成
- `コンフィグ項目.txt` のシステム/メッセージ/音量/言語を、TJS グローバル関数 `KrkrzAndroidGetConfig`/`SetConfig`(辞書)・`KrkrzAndroidGetVolume`/`SetVolume`(0-100) で取得・通知。**「定義されていれば呼ぶ」方式**なので TJS 側未実装でもクラッシュしない
- TJS 呼び出しは `TVPGetScriptDispatch()` でグローバル取得 → `drainRequests()` (continuous event hook = TJS スレッド) で実行。`JNI_OnLoad` は playgames が既定義のため定義せず、JavaVM は呼び出し時にキャッシュ
- 契約とキー一覧: `app/src/main/cpp/androidconfig/manual.tjs` / 参考実装: `sample_config.tjs` / 案件側の対応手順: `app/src/main/cpp/androidconfig/SETUP.md`
- **TJS 側の4関数はゲーム本体スクリプト (`assets/data/` = リポジトリ外) に属するので案件側で配線が必要**

`KrkrzActivity.onCreate` でプラグインの Java 側初期化フックを呼ぶ
(例: `PlayGamesBridge.initialize(this)`)。
ネイティブ側コールバックは `tTVPContinuousEventCallbackIntf` 上で
スレッドセーフキューを drain する設計 (`JniBridge.cpp` の `drainCallbacks`)。
JNI ネイティブメソッドの実装は `JNIEXPORT` で書いておけば
`krkrz_plugin` マクロが付ける `--whole-archive` リンカフラグで `libkrkrz.so` から露出される。

### ビルド設定の単一ソース

バージョン・SDK・パッケージ名は **`app-config.json` が単一ソース**。`app/build.gradle` が `JsonSlurper` でパースして `applicationId` / `versionCode` / `versionName` / `compileSdk` / `targetSdk` / `minSdk` / `ndkVersion` に流し込む。Makefile も `APP_ID` を同 JSON から正規表現で抜き出す。`build.gradle` を直接書き換えず、JSON を編集するのが正規ルート。

### Play Asset Delivery (PAD) 対応

大容量データ (xp3・動画, 数 GB) は APK 同梱では Play 配信不可なので、`app-config.json` の **`padPacks`** で Play Asset Delivery のパックとして配信する。`assetPack` (APK 同梱・小物用) はそのまま残し、xp3/動画を `padPacks` へ移す。要点:

- `settings.gradle` が `padPacks[].name` から `${PROJECT_DIR}/build/android/assetpacks/<name>/` に asset pack モジュール (`com.android.asset-pack`) を**動的生成 + include**。`app/build.gradle` が `android.assetPacks` に紐付け、`packPad_<name>` タスクで各モジュールの `src/main/assets/` へコピー。パック名は `[a-z0-9_]+`。
- `deliveryType` は `install-time` / `fast-follow` / `on-demand` (既定 on-demand)。fast-follow/on-demand は実ファイル展開され `assetsPath()` で絶対パスが取れる → 既存 `setAssetCacheDir()` にそのまま渡せる (**native 無改修・データコピー不要**)。install-time は AssetManager 名前空間なので既存 Bootstrap コピー経路が拾う。
- ランタイム: `BootstrapActivity` が `PadAssetResolver` で fetch パック (`R.array.pad_fetch_packs`) を取得 (進捗は既存 ProgressBar) し `<filesDir>/padroot/` に symlink 統合 → `KrkrzActivity` へ Intent extra `assetRoot` で渡す。`padPacks` 無しのサンプルは従来動作 (後方互換)。
- ローカル検証は `make pad-install` (bundletool `--local-testing`)、Play 用 AAB は `make pad-aab`。ビルド環境不要の配布用一式は `make pad-package`。詳細は `docs/android-play-asset-delivery.md`。
- **軽量イテレーション `make pad-fast`**: コードだけ直すとき用。素の `assembleDebug -PpadLocalTesting` (base APK に `local_testing_dir` meta-data を注入する debug manifest overlay `app/src/padlocal/AndroidManifest.xml`。release には載らない) を `adb install -r` し、pack は内容ハッシュ (`assetpacks/.fastpush/<name>.sha`) で差分判定して **変わった pack だけ** bundletool で再 push (+ 端末の `files/assetpacks/<name>` を消して再展開)。データ未変更なら pack 無操作で数秒。前提: 先に一度 `make pad-install`。**仕組み・処理構造・実機実測 (local-testing の meta-data / 端末レイアウト / 1-1 展開キーの落とし穴 / 差分判定) の詳細は `docs/android-pad-fast-iteration.md`**。adb でのファイル確認/削除は `docs/android-adb-tips.md`。
- 注意: PAD の symlink farm (`PadAssetResolver.buildSymlinkFarm`) は padroot 作り直し時に **ぶら下がり symlink** も消す必要があるため、削除判定は `Files.exists(..., NOFOLLOW_LINKS)` ベース (`File#exists()` は symlink を辿るので不可)。pack 差し替え時に踏む。

### REPL (adb 経由の TJS 対話シェル, Debug 専用)

Debug ビルドのみ `KRKRZ_REPL=ON` (`app/src/main/cpp/CMakeLists.txt`、icline は `KRKRZ_REPL_LINE_EDIT=OFF` で回避)。engine 側 `tTVPReplSocketChannel` (krkrz_dev, `common/utils/ReplSocketChannel.*`) が abstract unix socket で「1行 TJS → 1行 JSON」を返す。`KrkrzActivity` が debuggable 時に env `KRKRZ_REPL_SOCKET=<applicationId>.repl` を設定して起動。接続は `make repl` (nc) / `make repl-py` (Python 高機能クライアント)。操作は本家 console REPL 準拠 (`.help`/`.depth`/`.compact`・括弧が閉じるまで複数行・行末 `\` 継続)。Release には REPL は入らない。詳細は `docs/android-repl.md`。

### 署名

Release 署名は `local.properties` の `RELEASE_STORE_FILE` / `RELEASE_STORE_PASSWORD` / `RELEASE_KEY_ALIAS` / `RELEASE_KEY_PASSWORD` から読む (なければ同名の環境変数にフォールバック)。`local.properties` は `.gitignore` 済みなのでそこに置く。CI では環境変数経由で渡す。

## 編集時の注意

- `app/build.gradle` の `applicationId` 等を直接編集しない。`app-config.json` を更新する。
- ABI を増やす場合は CMakeLists.txt の triplet マッピングと `app/build.gradle` の `abiFilters` の両方を揃える。
- KrkrZ 本体・プラグインの実装に手を入れる必要がある場合、対象は `${KRKRZ_BASE}/krkrz_dev/` 配下であってこのリポジトリではない。リポジトリ内を grep しても見つからないのは仕様。
- `assets/data/startup.tjs` がアプリ起動時のエントリ。`app/src/main/assets/data/` は `.gitignore` 対象 (リポジトリにコミットしないテンプレート構成)。
