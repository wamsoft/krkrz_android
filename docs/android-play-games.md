# Google Play Games Services (Play Games) 対応

TJS から `PlayGames` クラスで Google Play Games (サインイン / 実績 / クラウドセーブ) を
使うための連携。**Play Games 対応をやるときはまずこのドキュメントを読む** — ここが入口で、
Play Console の具体手順・TJS レシピ・API リファレンスは末尾のリンクにまとめてある。

サンプル案件 (リポジトリ同梱の `app-config.json`) は既定で **有効**。

---

## 1. 有効 / 無効の切り替え (スイッチは 1 つ)

**`app-config.json` の `cmake.plugins` (と `staticPlugins`) に `"playgames"` があるかどうか、
それだけ**で Play Games 対応の有無が全部決まる。専用の設定キーは無い
(ネイティブ TJS プラグインの有無 = 機能全体の有無、という単一ソース)。

```jsonc
// 有効にする: 両方のリストに入れる
"cmake": {
  "plugins":       [ ..., "playgames", ... ],
  "staticPlugins": [ ..., "playgames", ... ]
}
```

`"playgames"` を **両リストから外すだけ** で無効化できる。`app/build.gradle` や
`AndroidManifest.xml`、Java を手で触る必要はない。

### 無効化したとき自動で起きること

`app/build.gradle` が `playGamesEnabled = plugins.contains('playgames')` を見て、無効時は:

- **gms 依存を付けない** — `play-services-games-v2` / `-auth` / `-tasks` は APK/AAB に一切入らない。
- **gms を使う Java をコンパイルしない** — `app/src/playgames/java`
  (`jp.wamsoft.krkrz.playgames.*` と `ui.PlayGamesAdminActivity`) をソースセットに加えない。
  代わりに `app/src/nogames/java` の **no-op stub** (`PlayGamesHook`) を使う。
- **Manifest から Play Games エントリを除去** — `AndroidManifest.xml` 内の
  `PLAYGAMES:BEGIN` / `PLAYGAMES:END` で囲んだブロック (gms メタデータ 2 つ + 管理画面 Activity)
  を取り除いた manifest を生成して使う。
- **drawer メニューの「Google Play Games」項目を実行時に非表示** (`PlayGamesHook.isEnabled()` が false)。

結果として、無効案件の APK/AAB には **gms コードも Play Games 関連の Manifest エントリも一切残らない**。

---

## 2. 構成 (どこに何があるか)

| 層 | 場所 | 有効時のみ? |
|----|------|:-:|
| ネイティブ TJS バインディング (`PlayGames` クラス) | `app/src/main/cpp/playgames/` (static plugin) | plugins で制御 |
| gms を使う Java (Bridge / Auth / Achievements / Snapshots) | `app/src/playgames/java/jp/wamsoft/krkrz/playgames/` | ✔ |
| 管理画面 UI (サインイン状態・実績・セーブ一覧) | `app/src/playgames/java/jp/wamsoft/krkrz/ui/PlayGamesAdminActivity.java` | ✔ |
| facade `PlayGamesHook` (実体) | `app/src/playgames/java/jp/wamsoft/krkrz/PlayGamesHook.java` | ✔ |
| facade `PlayGamesHook` (no-op stub、同名クラス) | `app/src/nogames/java/jp/wamsoft/krkrz/PlayGamesHook.java` | 無効時 |
| gms 依存 | `app/build.gradle` (`if (playGamesEnabled)`) | ✔ |
| Manifest エントリ | `app/src/main/AndroidManifest.xml` の `PLAYGAMES:BEGIN/END` ブロック | ✔ |
| 起動時初期化 / メニュー配線 | `KrkrzActivity` (常に `PlayGamesHook` 経由) | 常時 |

### なぜこの形か (仕組み)

- **gms を import する Java を `main` の外に出す** ことで、無効時にコンパイル対象から外し、
  gms 依存ごと落とせるようにしている (`main` に置くと常にコンパイルされ gms が必須になる)。
- **`KrkrzActivity` は facade `PlayGamesHook` しか参照しない**。実体 (playgames 側) と
  stub (nogames 側) が同じクラス名で、ビルドごとにどちらか一方だけがソースセットに入るので、
  有効・無効どちらでも `KrkrzActivity` はそのままコンパイルできる (リフレクション不要)。
- Manifest の `com.google.android.gms.version` メタデータは
  `@integer/google_play_services_version` を参照し、この整数リソースは **gms 依存が供給** する。
  無効時に残すと未定義参照でビルドが落ちるため、マーカーで囲んで strip している。

---

## 3. 使うとき (有効化 + Play Console)

1. `app-config.json` の `plugins` / `staticPlugins` に `"playgames"` を入れる (§1)。
2. Play Console で **App ID** を発行し、`strings.xml` (または `resOverride`) の
   `games_app_id` を実 ID に書き換える。`0000000000` のままだとビルドは通るが
   `PlayGames` API はすべて失敗する。
3. OAuth クライアントに署名鍵の **SHA-1** を登録し、**テスター**を追加する
   (これらを忘れると `signIn()` が必ず失敗する)。→ 詳細は SETUP.md。
4. TJS から `PlayGames.*` を呼ぶ (初期化は `KrkrzActivity` が自動で行う)。→ レシピは USAGE.md。

動作確認は、アプリ起動 → 左端スワイプ →「Google Play Games」の管理画面で
サインイン状態 / 実績 / セーブ一覧を Java SDK 経由で直接確認できる。

> `releaseDebugSigned` (debug 鍵署名のリリース構成) で確認する場合は、debug キーストアの
> SHA-1 も Play Console に登録しておくこと。

---

## 4. 詳細リンク

- **Play Console セットアップの手順 / テストチェックリスト / トラブルシュート / 拡張ロードマップ**:
  [`app/src/main/cpp/playgames/SETUP.md`](../app/src/main/cpp/playgames/SETUP.md)
- **TJS レシピ集** (実績解除・クラウドセーブ・起動時フロー):
  [`app/src/main/cpp/playgames/USAGE.md`](../app/src/main/cpp/playgames/USAGE.md)
- **TJS API リファレンス**:
  [`app/src/main/cpp/playgames/manual.tjs`](../app/src/main/cpp/playgames/manual.tjs)
- **JNI プラグインを新規に追加するときの汎用手順** (Play Games と同じ Java 連携パターン):
  [`docs/android-jni-plugin.md`](android-jni-plugin.md)
