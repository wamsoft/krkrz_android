# PlayGames プラグイン 運用手順書

このプラグインは実装完了済みだが、**実機で動かすには Google Play Console 側のセットアップ
と、いくつかのアプリ側設定が必須**。本ドキュメントは順番にやれば動くところまで持っていける手順と、
動かなかったときのチェックリスト、今後の拡張計画をまとめている。

API 仕様自体は同フォルダの `manual.tjs` を参照。

---

## 1. Play Console セットアップ (一度だけ)

### 1-1. ゲームサービスの作成

1. [Google Play Console](https://play.google.com/console/) にアクセスし対象アプリを選択
   (まだ APK をアップロードしていない場合でも、アプリ作成だけ済ませておけばゲームサービスは設定できる)
2. 左メニュー「拡張機能」→ 「Play ゲームサービス」→ 「設定と管理」→ 「構成」
3. 「Play ゲームサービスを使用しない、新しい Play ゲームサービス プロジェクトを作成」を選択
4. プロジェクト名・名前空間 (アプリ ID) を入力して作成

完了すると **App ID** (10 桁前後の数値、例 `1234567890`) が発行される。

### 1-2. 認証情報の登録

1. 「資格情報」タブ → 「認証情報を追加」
2. 「種類」: **Android**
3. 「認証情報名」: 任意
4. パッケージ名: `app-config.json` の `applicationId` と完全一致させる
5. **OAuth クライアント ID**: 「Google Cloud Platform に移動して作成」リンクから OAuth クライアントを作成
   - アプリケーションタイプ: Android
   - SHA-1 証明書フィンガープリント: 後述の方法で取得して登録

### 1-3. SHA-1 フィンガープリントの取得

ビルド種別ごとに別の SHA-1 が必要。**Debug と Release で別々に登録**する (両方の認証情報を登録できる)。

```bash
# Debug キーストア (デフォルト)
keytool -list -v \
  -keystore ~/.android/debug.keystore \
  -alias androiddebugkey \
  -storepass android -keypass android \
  | grep SHA1

# Release キーストア (release-key.jks を作成済みの場合)
keytool -list -v \
  -keystore release-key.jks \
  -alias my-key-alias \
  | grep SHA1
```

**Play App Signing を使う場合**: Play Console 側で「アップロード鍵」と「アプリ署名鍵」両方の SHA-1
が表示されるので、両方とも OAuth クライアントに登録しておく。

### 1-4. テスター登録

リリース公開前は、Play Games Services は **テスターとして登録されたアカウントのみ** で動作する。

1. Play Console > 「Play ゲームサービス」> 「テスト」
2. 「テスターを追加」で動作確認に使う Google アカウントのメールアドレスを追加

これを忘れると `signIn()` が常に `SIGN_IN_REQUIRED` (10) や `DEVELOPER_ERROR` (10) で失敗する。

### 1-5. 実績 / セーブゲームを使う場合

#### 実績

1. Play Console > 「実績」タブ > 「実績を追加」
2. 1 つ作成すると `CgkI...` 形式の **Achievement ID** が発行される
3. TJS 側ではこの ID を `unlockAchievement(cb, "CgkI...")` のように渡す

#### セーブゲーム (Snapshots)

1. Play Console > 「ゲームの詳細」または「構成」タブで、「保存されたゲーム」を **有効化**
2. 有効化しないと `writeSnapshot` 系が `MISCONFIGURED` で全部失敗する
3. ID は使わず、TJS 側で任意の `uniqueName` (英数字+一部記号、最大 100 文字) を指定するだけ

---

## 2. アプリ側設定

### 2-1. App ID を `strings.xml` に書く

`app/src/main/res/values/strings.xml`:

```xml
<string name="games_app_id" translatable="false">1234567890</string>
```

`0000000000` のままだと SDK 初期化時に warning が出るだけで APK ビルドは通る。
ただし `signIn()` などの API はすべて失敗するので必ず実 ID に書き換えること。

### 2-2. パッケージ名を確認

`app-config.json` の `applicationId` が、Play Console の認証情報で登録したパッケージ名と
**完全一致** していることを確認。違うと `DEVELOPER_ERROR` で死ぬ。

### 2-3. 署名鍵を確認

`installDebug` で実機にインストールするときの署名鍵 (debug.keystore) と、
Play Console に登録した SHA-1 が一致していることを確認。

---

## 3. ビルド & 動作確認

### 3-1. ビルド

```bash
make build              # Debug
# または
./gradlew :app:assembleDebug
```

成功すると `app/build/outputs/apk/debug/app-debug.apk` が出力される。

### 3-2. 実機/エミュレータにインストール

```bash
make run                # install + LAUNCHER 起動
# または
make install
```

エミュレータの場合、**Google Play Services が入った AVD** を選ぶこと
(AVD 名に `(Google Play)` が付いているもの。`(Google APIs)` だけのものは Play Games Services 不可)。

### 3-3. ログ確認

別ターミナルで:

```bash
make logcat | grep -E "playgames|PlayGames|GamesSignIn"
```

成功時のログ例:

```
PlayGamesBridge: PlayGamesSdk initialized
playgames: JNI_OnLoad: JavaVM cached
```

### 3-4. TJS から疎通確認

`app/src/main/assets/data/startup.tjs` (または既存の起動 TJS) から:

```tjs
PlayGames.isAuthenticated(function(authed, err) {
    if (err !== void) {
        Debug.message("PlayGames error: " + err);
        return;
    }
    Debug.message("PlayGames signed in: " + authed);
    if (!authed) {
        PlayGames.signIn(function(ok, err2) {
            Debug.message("signIn result: " + ok + " err=" + err2);
        });
    }
});
```

---

## 4. テスト用チェックリスト

実装が動いているか段階的に確認するためのチェックリスト。
各項目はそれより前の項目が成功している前提。

### Phase A: 基盤

- [ ] `make build` がエラーなく通る
- [ ] APK インストール後、アプリが起動して通常の TJS スクリプトが動く
  (= `libkrkrz.so` のロードが壊れていない)
- [ ] logcat に `PlayGamesSdk initialized` が出る (= Java 側初期化成功)
- [ ] logcat に `JNI_OnLoad: JavaVM cached` が出る (= JNI 初期化成功)

### Phase B: 認証

- [ ] `PlayGames.isAuthenticated(cb)` を呼んで `cb(true, void)` か `cb(false, void)` が返る
- [ ] `false` だった場合 `PlayGames.signIn(cb)` で Google の sign-in UI が表示される
- [ ] sign-in UI で「続行」を押すと `cb(true, void)` が返る
- [ ] `PlayGames.getCurrentPlayer(cb)` で playerId / displayName を含む辞書が返る

### Phase C: 実績 (要 Play Console で実績作成)

- [ ] `PlayGames.loadAchievements(cb)` で実績配列が返る
- [ ] `PlayGames.unlockAchievement(void, "CgkI...")` でクラッシュしない
- [ ] 解除直後に通知トーストが画面上部に表示される
- [ ] `PlayGames.showAchievementsUI(cb)` でゲーム内実績一覧画面が出る

### Phase D: セーブゲーム (要 Play Console で「保存されたゲーム」有効化)

- [ ] `PlayGames.writeSnapshot(cb, "test01", "hello", "test slot")` で `cb(true, void)`
- [ ] `PlayGames.listSnapshots(cb)` に `test01` が入っている
- [ ] `PlayGames.readSnapshot(cb, "test01")` で `data` フィールドが正しく取得できる
- [ ] `PlayGames.deleteSnapshot(cb, "test01")` で削除でき、次の `listSnapshots` には現れない
- [ ] `PlayGames.showSnapshotsUI(cb, "セーブ選択")` でセーブ選択 UI が出る

---

## 5. トラブルシューティング

| 症状 | 主な原因 | 対処 |
|------|---------|------|
| `signIn` で `SIGN_IN_REQUIRED` (4) | 未テスター登録 | 1-4 でテスター追加 |
| `signIn` で `DEVELOPER_ERROR` (10) | SHA-1 不一致 / パッケージ名不一致 / OAuth クライアント未作成 | 1-2, 1-3 をやり直し |
| `signIn` で `INTERNAL_ERROR` (8) | App ID 不一致、または Play Services 不在の AVD | strings.xml の games_app_id 確認 / Google Play 入りの AVD で確認 |
| 全API で `PlayGamesBridge not initialized` | KrkrzActivity onCreate で initialize 呼ばれていない | KrkrzActivity を継承したアプリの場合 super.onCreate を確実に呼ぶ |
| 実績 unlock 直後に解除されない | Play Console で実績が「公開済み」になっていない | 実績作成画面下の「公開」ボタンを押す |
| `writeSnapshot` で `MISCONFIGURED` | 「保存されたゲーム」未有効化 | Play Console 「ゲームの詳細」で有効化 |
| `Java_jp_wamsoft_krkrz_playgames_PlayGamesNative_nativePostResult が見つからない` | プラグインが `libkrkrz.so` に組み込まれていない | `myapp.cmake` の `TVP_PLUGINS_STATIC` に `playgames` があるか確認 → `make clean && make build` |
| TJS 側で `PlayGames` クラスが未定義 | プラグイン NCB 登録が走っていない | logcat で `JNI_OnLoad` ログを確認、出ていなければビルドが反映されていない可能性 |

### logcat で見るキー

```bash
# プラグイン全体
make logcat | grep -E "playgames|PlayGamesBridge"

# Play Games SDK 内部 (失敗の本当の原因が判ることが多い)
make logcat | grep -E "GamesSignIn|GooglePlayServicesUtil|GoogleApiAvailability"

# 全部
make logcat | grep -i -E "play|games|sign"
```

---

## 6. 拡張ロードマップ

### 6-1. 短期 (Phase 1 で先送りした項目)

| 項目 | 必要な作業 |
|------|-----------|
| **リーダーボード** | `PlayGamesLeaderboards.java` 追加 (`LeaderboardsClient` で submitScore / load / show UI) → C++ 側ラッパー追加 → `Main.cpp` の NCB 登録に追記 |
| **showSnapshotsUI の選択結果取得** | `KrkrzActivity` で `onActivityResult` をオーバーライド (request code `0xCAFE0002`) して、Intent から選択された snapshot 名を取り出し、コールバックに返す |
| **Friends API** | `play-services-games-v2` の `PlayersClient.loadFriends` 経由 |

### 6-2. 中期 (Phase 2: 汎用 Java→TJS ブリッジ化)

現状の `JniBridge.cpp` は **Java→Native の片方向** (postResult/postError 受け側) は汎用化されているが、
**Native→Java の呼び出し側** (`CallStaticVoidMethod` のシグネチャ文字列をハンドコード) は
プラグインごとに書く必要がある。
これを以下の方向で抽象化する案がある:

#### 案 A: リフレクション式 JNI プラグインローダー

```cpp
// 吉里吉里コアの新コンポーネント
TVPRegisterAndroidPlugin("jp.wamsoft.krkrz.playgames.PlayGamesAuth");
```

- Java クラスの全 public static メソッドをリフレクションで列挙
- 各メソッドを TJS のクラスメソッドとして登録
- 呼び出し時に引数 `tTJSVariant` をリフレクション API (`Method.invoke`) 経由で渡す
- 戻り値は `jobjectToVariant` で逆変換

**Pros**: プラグインの C++ 側がほぼゼロになる (このフォルダの `PlayGames*.cpp` 全削除可能)
**Cons**: リフレクションのオーバーヘッド、シグネチャエラーが実行時にしか出ない

#### 案 B: コード生成

Java クラスにアノテーション `@TJSMethod` を付け、`build.gradle` の処理で
JNI ブリッジ C++ をビルド時自動生成する。

**Pros**: 静的型チェック、ランタイムオーバーヘッドなし
**Cons**: 生成処理の実装コストが大きい

#### 想定方針

実機での Phase 1 動作確認後に着手判断。
2 個目の Java バインディングプラグイン (例: AdMob, Billing) を **手書きで** 作ってから
共通パターンを抽出して案 A を実装するのが、抽象化のミスマッチを避けやすい。

### 6-3. 長期

- iOS GameCenter プラグインを同じ TJS インタフェース (`PlayGames` クラス名は変わるが
  メソッド体系を揃える) で実装し、TJS 側スクリプトをほぼそのまま流用できるようにする
- セーブデータ暗号化 / バージョン管理レイヤーの追加 (アプリごとの責務として lib 化検討)

---

## 7. 機能オフ手順

**`app-config.json` の `cmake.plugins` と `staticPlugins` から `"playgames"` を外すだけ**。
これだけで `app/build.gradle` が gms 依存・gms を使う Java (`app/src/playgames/java`)・
Manifest の Play Games エントリ・drawer メニュー項目を自動的に全部落とす
(`app/build.gradle` / `AndroidManifest.xml` / Java を手で編集する必要は無い)。

プロジェクトを切り替えたら `make clean-cmake` を推奨。仕組みの詳細は
[`docs/android-play-games.md`](../../../../../docs/android-play-games.md) を参照。
