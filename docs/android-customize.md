# 自分のアプリにする (名前・アイコン・Activity・JNI)

自分のタイトルとして出すときに変えるところです。アプリ ID・バージョン・プラグイン・資材は
`app-config.json` で指定します ([設定](android-config.md))。ここではそれ以外を説明します。

## アプリ名

`app/src/main/res/values/strings.xml` の `app_name` です。

```xml
<resources>
    <string name="app_name">あなたのアプリ名</string>
</resources>
```

リポジトリのファイルを直接変えずに、`resOverride` のフォルダに `values/strings.xml` を置いて
`app_name` だけを上書きすることもできます ([設定](android-config.md) の resOverride)。

## アイコン

アイコンは `app/src/main/res/` の `mipmap-*` フォルダにあり、`AndroidManifest.xml` で次のように指定しています。

- 通常のアイコン: `@mipmap/ic_launcher`
- 丸型のアイコン: `@mipmap/ic_launcher_round` (Android 7.1 以降)

各フォルダの `ic_launcher.png` と `ic_launcher_round.png` を差し替えます
(`resOverride` のフォルダに置けば、リポジトリのファイルは変えずに済みます)。

| フォルダ | 大きさ |
|---|---|
| `mipmap-mdpi/` | 48×48 |
| `mipmap-hdpi/` | 72×72 |
| `mipmap-xhdpi/` | 96×96 |
| `mipmap-xxhdpi/` | 144×144 |
| `mipmap-xxxhdpi/` | 192×192 |

Android Studio の "Image Asset Studio" を使うと、これらを一度に作って置けます。

### アダプティブアイコンも上書きする (密度別の PNG だけでは変わらない)

`app/src/main/res/mipmap-anydpi/ic_launcher.xml` に既定の**アダプティブアイコン** (`<adaptive-icon>`) が
入っています。Android 8.0 (API 26) 以降はこの `anydpi` の定義が密度別の PNG より**優先される**ので、
密度別の PNG だけを差し替えても**アイコンは変わりません**。

`resOverride` のフォルダに **`mipmap-anydpi-v26/ic_launcher.xml` と `ic_launcher_round.xml`** も置いて
上書きしてください (`-v26` 付きは無印の `anydpi` より優先されます)。

できあがった不透明な PNG をそのまま見せたい場合は、背景レイヤに全面で出すのが手軽です
(前景は透明。ランチャーの形に切り抜かれます)。

```xml
<!-- <resOverride のフォルダ>/mipmap-anydpi-v26/ic_launcher.xml (round も同様) -->
<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">
    <background android:drawable="@mipmap/ic_launcher" />
    <foreground android:drawable="@android:color/transparent" />
</adaptive-icon>
```

きちんとアダプティブに対応する場合は、前景 (安全領域に収まる図柄) と背景を分けて用意します。
反映されたかは `aapt dump badging <apk> | grep icon` で、`icon=` が `mipmap-anydpi-v26/...` を
指していれば OK です。

## 独自の Activity

既定の `KrkrzActivity` を継承して、独自の初期化や機能を足せます。

1. `app/src/main/java/` の下にパッケージ名に合わせたフォルダを作ります
   (例: パッケージ名が `com.example.mygame` なら `app/src/main/java/com/example/mygame/`)
2. `jp.wamsoft.krkrz.KrkrzActivity` を継承したクラスを作ります

   ```java
   package com.example.mygame;

   import android.os.Bundle;
   import jp.wamsoft.krkrz.KrkrzActivity;

   public class MainActivity extends KrkrzActivity {
       @Override
       protected void onCreate(Bundle savedInstanceState) {
           super.onCreate(savedInstanceState);
           // ここに独自の初期化処理を書く
       }
   }
   ```

3. `app/src/main/AndroidManifest.xml` の `<activity>` の `android:name` を、作ったクラス名にします

   ```xml
   <activity
       android:name="com.example.mygame.MainActivity"
       android:exported="true"
       android:configChanges="layoutDirection|locale|orientation|..."
       >
       <!-- ... -->
   </activity>
   ```

## ネイティブコード (JNI) で関数を足す

吉里吉里Z の機能とは別に、C/C++ で独自の処理を書く場合の手順です。TJS から呼べるプラグインとして
作る場合は [JNI プラグイン作成手順](android-jni-plugin.md) を参照してください。

1. `app/src/main/cpp/` に C++ のソース (例: `native-lib.cpp`) を置きます
2. `app/src/main/cpp/CMakeLists.txt` にビルドの設定を足します

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

3. 継承した Activity でライブラリを読み、ネイティブメソッドを宣言します

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
