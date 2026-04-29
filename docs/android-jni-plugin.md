# Android JNI プラグイン作成手順

Android 専用で **Java SDK を経由するプラグイン** を吉里吉里 Z 用に書くときの
標準パターンと手順。Play Games プラグイン
(`app/src/main/cpp/playgames/` + `app/src/main/java/jp/wamsoft/krkrz/playgames/`)
を最初の事例として実装したものを基にしている。

## 1. このパターンが向いているケース

- Java/Kotlin SDK しか提供されていない (例: Google Play Games / AdMob / Firebase / In-App Billing)
- Android 固有の Activity / Context が必要 (例: 共有シート起動、システム設定読み書き)
- 同期的な C/C++ ブリッジでも書けるが、Java 側にロジックを置く方が SDK 利用がシンプルになるもの

逆に **Steam プラグインのような C/C++ 完結のものは従来どおり** `krkrz_dev/src/plugins/<name>/`
に置く。Android 固有のものだけ `krkrz_android/app/` に置く方針。

## 2. 全体構造

```
app/
├── build.gradle                              # SDK 依存を直接追加
├── src/main/
│   ├── AndroidManifest.xml                   # 必要なら meta-data / permission 追記
│   ├── res/values/strings.xml                # API キー等のリソース
│   ├── java/jp/wamsoft/krkrz/<name>/         # Java 実装
│   │   ├── <Name>Bridge.java                 # Activity 保持・SDK 初期化
│   │   ├── <Name>Native.java                 # native コールバック宣言
│   │   └── <Name><Subsystem>.java            # 機能ごとの Java 側実装
│   └── cpp/<name>/                           # C++ 実装
│       ├── CMakeLists.txt                    # krkrz_plugin マクロで static plugin 化
│       ├── Main.cpp                          # TJS クラス登録、PreRegist/PostRegist
│       ├── JniBridge.cpp/h                   # JavaVM 取得・型変換・コールバックキュー
│       └── <Name><Subsystem>.cpp/h           # 機能ごとの JNI 呼出ラッパー
└── ...

myapp.cmake                                   # TVP_PLUGINS_STATIC に追加
```

## 3. 手順

### Step 1: プラグイン名決定

スネークケース小文字を推奨 (例: `playgames`, `admob`, `billing`)。
このまま CMake のターゲット名 / フォルダ名 / Java パッケージ名末尾に使う。

### Step 2: Gradle 依存追加

`app/build.gradle` に必要な SDK 依存を直接追加:

```groovy
dependencies {
    // ... 既存
    implementation 'com.example.sdk:foo:1.0.0'
}
```

### Step 3: Java 側スケルトン作成

`app/src/main/java/jp/wamsoft/krkrz/<name>/` に最低限以下を置く。

#### `<Name>Native.java`

```java
package jp.wamsoft.krkrz.<name>;

public final class <Name>Native {
    private <Name>Native() {}
    public static native void nativePostResult(long callbackId, Object result);
    public static native void nativePostError(long callbackId, String errorMessage);
}
```

#### `<Name>Bridge.java`

```java
package jp.wamsoft.krkrz.<name>;

import android.app.Activity;

public final class <Name>Bridge {
    private static volatile Activity sActivity;

    public static synchronized void initialize(Activity activity) {
        sActivity = activity;
        // SDK 初期化処理 (例: SomeSdk.initialize(activity))
    }

    public static Activity activity() { return sActivity; }

    static void postResult(long cb, Object v) {
        if (cb != 0) <Name>Native.nativePostResult(cb, v);
    }
    static void postError(long cb, String m) {
        if (cb != 0) <Name>Native.nativePostError(cb, m != null ? m : "(null)");
    }
}
```

#### 機能ごとの Java クラス

各機能 (1 サブシステム ≒ 1 ファイル) は `<Name>Bridge.activity()` 経由で SDK を呼び、
Task/Future/Listener の完了で `<Name>Bridge.postResult/postError` を呼ぶ。

### Step 4: KrkrzActivity から SDK 初期化

`app/src/main/java/jp/wamsoft/krkrz/KrkrzActivity.java` の `onCreate` で
`<Name>Bridge.initialize(this)` を呼ぶ。

### Step 5: C++ プラグインフォルダ作成

`app/src/main/cpp/<name>/CMakeLists.txt`:

```cmake
set(TPSTUB_DIR ${KRKRZ_BASE}/krkrz_dev/src/plugins/tp_stub CACHE PATH "Path to tp_stub directory")
include(${TPSTUB_DIR}/krkrz.cmake)

if(NOT ANDROID)
    message(STATUS "<name> plugin: skipped (non-Android target)")
    return()
endif()

krkrz_plugin(<name>
    NCBIND
    SOURCES
        Main.cpp
        JniBridge.cpp
        <Name><Subsystem>.cpp
        # ...
    LIBRARIES
        log
        android
)
```

### Step 6: JniBridge.cpp/h を流用

playgames プラグインの `JniBridge.cpp/h` をそのままコピーして
名前空間 `playgames` を `<name>` に書き換える。
共通化したい場合は将来 [Phase 2 抽象化](#phase-2-将来-javatjs-汎用ブリッジ化) で対応。

JniBridge には以下が含まれる (=自分で書く必要なし):
- `ScopedEnv` (JNIEnv 取得 / AttachCurrentThread)
- `ttstrToJString` / `jStringToTtstr`
- `jobjectToVariant` (Boolean/Integer/Long/Float/Double/String/byte[]/Object[]/Map<String,?> → tTJSVariant)
- `variantToJObject` (Long/Double/String/byte[] → Java)
- `registerCallback` / `postResult` / `postError` / `drainCallbacks`
- `JNI_OnLoad` で JavaVM キャッシュ
- `nativePostResult` / `nativePostError` JNI export

**注意**: 1 つの `libkrkrz.so` に `JNI_OnLoad` は 1 つしか持てない。
他の Android プラグインを同居させる場合は `JNI_OnLoad` の定義を共通化する必要がある
(後述 Phase 2)。当面は **`JNI_OnLoad` を持つプラグインを 1 つに絞り**、
後発のプラグインからはそちらの `setJavaVm` を呼ぶ形にする。

### Step 7: 機能ごとの JNI 呼出ラッパー (C++)

`<Name><Subsystem>.cpp` でやることは Java の static メソッドを呼ぶだけ:

```cpp
tjs_error CallSomething(const tTJSVariantClosure &cb, const ttstr &arg1) {
    ScopedEnv se;
    if (!se.ok()) return TJS_E_FAIL;
    JNIEnv *env = se.env();
    jclass cls = getClassCache(env)->cls<Name>;   // JniBridge.h に追加しておく
    if (!cls) return TJS_E_FAIL;
    jmethodID mid = env->GetStaticMethodID(cls, "doSomething", "(JLjava/lang/String;)V");
    if (!mid) { if (env->ExceptionCheck()) env->ExceptionClear(); return TJS_E_FAIL; }
    tjs_int64 cbid = registerCallback(cb);
    jstring jarg = ttstrToJString(env, arg1);
    env->CallStaticVoidMethod(cls, mid, (jlong)cbid, jarg);
    if (jarg) env->DeleteLocalRef(jarg);
    if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); return TJS_E_FAIL; }
    return TJS_S_OK;
}
```

**JNI シグネチャ早見表** (頻出のもの):

| Java 引数            | JNI Signature                |
|---------------------|------------------------------|
| void                | `V`                          |
| boolean             | `Z`                          |
| int                 | `I`                          |
| long                | `J`                          |
| float / double      | `F` / `D`                    |
| String              | `Ljava/lang/String;`         |
| byte[]              | `[B`                         |
| String[]            | `[Ljava/lang/String;`        |
| Object              | `Ljava/lang/Object;`         |
| Map<String,Object>  | `Ljava/util/Map;`            |

複数引数なら `(JLjava/lang/String;I)V` のように並べる。
戻り値型は最後の `)` の後に書く。

### Step 8: Main.cpp で TJS クラス登録

playgames プラグインの `Main.cpp` を雛形に:

- 静的シングルトンクラスを Factory 禁止で登録 (`Factory(&Class::Factory)` で例外投げ)
- `tTVPContinuousEventCallbackIntf` で `<name>::drainCallbacks()` を毎フレーム呼ぶ
- `NCB_REGISTER_CALLBACK(PreRegist, ...)` でフックを登録、`PostRegist` で `shutdownCallbacks` も
- `NCB_METHOD` で固定引数のメソッド、`RawCallback("name", ..., TJS_STATICMEMBER)` で可変引数

### Step 9: myapp.cmake に登録

```cmake
LIST(APPEND TVP_PLUGIN_FOLDERS
  ${CMAKE_CURRENT_LIST_DIR}/app/src/main/cpp        # 既に追加済みなら省略
)

LIST(APPEND TVP_PLUGINS
  <name>
)

LIST(APPEND TVP_PLUGINS_STATIC
  <name>
)
```

### Step 10: ビルド & 動作確認

```bash
make clean && make build
make logcat | grep <name>
```

`libkrkrz.so` に JNI シンボルが入っているか確認:

```bash
NDK_NM=$HOME/AppData/Local/Android/Sdk/ndk/<version>/toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-nm.exe
$NDK_NM -D --defined-only \
  app/build/intermediates/cxx/Debug/*/obj/arm64-v8a/libkrkrz.so \
  | grep "Java_jp_wamsoft_krkrz_<name>"
```

## 4. コールバックモデル

すべての非同期メソッドは TJS から `(callback) -> ...` の形で呼ばれる。
callback は `function(result, error)` シグネチャ:

- 成功: `result=値, error=void`
- 失敗: `result=void, error=ttstr (エラーメッセージ)`

Java 側 (一方の終端) は最後に `<Name>Bridge.postResult(callbackId, value)` または
`postError(callbackId, message)` を呼ぶ。

C++ 側 (もう一方の終端) は `JniBridge.cpp` の `drainCallbacks()` が
`tTVPContinuousEventCallbackIntf` で TJS スレッドから呼ばれ、
キューに積まれた結果を順に元の closure に流す。
**Play Games SDK のコールバックは UI スレッドだが TJS スレッド (= SDL スレッド) で
コールバックが呼ばれることが保証されている** ので、TJS 側は素直に書ける。

### Fire-and-forget

callback に `void` を渡せば、結果通知不要なメソッドにできる
(Java 側で callbackId == 0 を見て postResult を呼ばない実装にする)。
例: `PlayGames.unlockAchievement(void, "CgkI...")`

## 5. 型マーシャリング対応表

`JniBridge::jobjectToVariant` 経由で Java → TJS にこのまま渡せる:

| Java 型             | TJS 型             | 備考 |
|--------------------|--------------------|------|
| `null`             | void               |      |
| `Boolean`          | int (0/1)          |      |
| `Integer` / `Long` | integer            |      |
| `Float` / `Double` | real               |      |
| `String`           | string (ttstr)     | UTF-8 経由 |
| `byte[]`           | octet              |      |
| `Object[]`         | TJS Array          | 要素は再帰変換 |
| `Map<String, ?>`   | TJS Dictionary     | キーは String 限定、値は再帰変換 |
| その他              | string (toString)  | フォールバック |

`variantToJObject` (TJS → Java) は最低限の対応のみ:

| TJS 型          | Java 型          |
|-----------------|-----------------|
| void            | null            |
| integer         | Long            |
| real            | Double          |
| string          | String          |
| octet           | byte[]          |
| object          | null (未対応)   |

ネイティブ→Javaの方向で複雑な構造体を渡す必要が出たら、
個別に Java 側で String JSON や Bundle として受ける設計にするか、
JniBridge を拡張する。

## 6. JNI ハマりどころ

### 6-1. Local reference の解放

`env->DeleteLocalRef(jstr)` を忘れると 1 ループで一杯になる
(デフォルト 512 個)。`jstring`, `jobjectArray`, 個別 `jobject` を作ったら
**スコープ末尾で必ず DeleteLocalRef**。

### 6-2. Exception 残り

JNI 呼出後に Java 側で例外が起きていると、それを処理しないまま次の JNI 呼出をすると即クラッシュ。
全 JNI 呼出後に:

```cpp
if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    return TJS_E_FAIL;
}
```

### 6-3. Class 参照のグローバル化

`FindClass()` で取れる `jclass` は local reference。
頻繁に使うクラスは `NewGlobalRef` してキャッシュし、プラグイン unload 時に DeleteGlobalRef する。
`JniBridge.cpp` の `getClassCache()` でこのパターンを実装済み。

### 6-4. スレッド

Play Games SDK のコールバックは UI スレッド。
TJS は SDL のメインスレッド。
コールバックを直接 TJS から呼ぶと **必ずクラッシュする**。
`JniBridge` のキュー経由 (`postResult`/`drainCallbacks`) を必ず使うこと。

### 6-5. `JNI_OnLoad` の競合

Android では 1 つの `.so` に `JNI_OnLoad` を 1 つしか持てない (linker error)。
`libkrkrz.so` に静的リンクされる Android プラグインが複数になったら、
`JNI_OnLoad` を持つのは 1 つに絞り、他は `setJavaVm()` を共有する形にする
(将来 Phase 2 で `krkrz_dev/src/core` 側に移すのが望ましい)。

## 7. Phase 2: 将来 Java↔TJS 汎用ブリッジ化

複数の Java バインディングプラグインを抱えるようになったら、
`JniBridge.cpp` の機能を `krkrz_dev/src/core/generic/jniBridge/` に移動して
共通化する案がある。検討点:

- **`JNI_OnLoad` を core が持つ**: 各プラグインは `JNI_OnLoad` を持たず、
  core から `setJavaVm` を呼ばれた状態で初期化される
- **Java クラスのリフレクションベース TJS 公開**:
  `TVPRegisterAndroidPlugin("jp.wamsoft.krkrz.<name>.<Class>")` を呼ぶだけで
  全 public static メソッドを TJS から呼べるようにする。プラグイン側 C++ コードがほぼゼロになる。

詳細は `app/src/main/cpp/playgames/SETUP.md` 6-2 節を参照。
