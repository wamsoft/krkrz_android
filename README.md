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

トップフォルダ myapp.cmake で、同時リンクする
対象プラグインを追加できます

TVP_PLUGIN_FOLDERS  プラグインが存在するフォルダ名を追加
TVP_PLUGINS         プラグイン名列挙

### 1. ライブラリの配置

SDLの `.aar` ファイルを `app/libs/` ディレクトリに配置してください。

* `SDL3-x.x.x.aar` (SDL3 ライブラリ)

※ `app/build.gradle` の設定により、`libs` フォルダ内のすべての `.aar` ファイルが自動的に読み込まれます。

install-sdl-aar.sh で指定バージョンの SDLをダウンロードして配置できます

### 2. リソース・スクリプトの配置

吉里吉里Zの実行に必要なデータは `app/src/main/assets/` 以下に配置します。

* **スクリプト・データ**: `app/src/main/assets/data/` 以下に配置するとその中の `startup.tjs`から起動します

## ビルドと実行 (Gradle)

プロジェクトのルートディレクトリで以下のコマンドを実行してください。

### デバッグビルド (APK生成)
```bash
./gradlew assembleDebug
```
生成された APK は `app/build/outputs/apk/debug/` に出力されます。

### デバイスへのインストール (デバッグ)
Android デバイスを接続した状態で実行します。
```bash
./gradlew installDebug
```

### リリースビルド
```bash
./gradlew assembleRelease
```
※ リリースビルドを行うには、別途署名設定 (Signing Config) が必要になる場合があります。

## 自前アプリへのカスタマイズ

独自のアプリとしてリリースする場合、以下のファイルを修正してください。

### 1. アプリID・バージョン情報の変更
プロジェクトルートの `app-config.json` ファイルを編集してください。

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
- install/reinstall/uninstall: 端末へのインストール/再インストール/アンインストール
- run: ランチャーアクティビティを起動
- debug: デバッガ待機で起動 (`am start -D`)、後からアタッチ
- stop: アプリの強制停止
- logcat: `adb logcat` のフォロー
- clean: クリーニング
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
