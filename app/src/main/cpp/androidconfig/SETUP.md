# androidconfig — 設定画面と吉里吉里本体コンフィグの連携 手順書

Android の「機能設定」画面に、`コンフィグ項目.txt` のシステム/メッセージ/音量/言語の
各項目を ON/OFF スイッチ・スライダー・ドロップダウンで出し、吉里吉里本体 (TJS) 側と
**辞書で取得 / 辞書で通知 / 音量は直接 get・set** という形で同期するための仕組み。

- 契約 (呼び出すグローバル関数とキー一覧): [`manual.tjs`](manual.tjs)
- 参考実装 (雛形): [`sample_config.tjs`](sample_config.tjs)
- 本書: タイトル側で「実際に動かす」ための手順

---

## 1. 仕組みの全体像

```
                      Android (Java / UI スレッド)            native plugin              吉里吉里本体 (TJS スレッド)
 設定画面を開く ─ KrkrzConfig.requestConfig() ─▶ queue ─┐
                                                         │  drainRequests()
                                                         └─ continuous event hook ─▶ KrkrzAndroidGetConfig()
                                                                                      KrkrzAndroidGetVolume(key) × N
 UI を動的構築 ◀─ onConfigReady(config, volumes) ◀───────  (Handler で UI スレッドへ)
 スライダー操作 ─ KrkrzConfig.setVolume(key,val) ─▶ queue ─▶ drain ─▶ KrkrzAndroidSetVolume(key, val)   ← ライブ反映
 画面を閉じる   ─ KrkrzConfig.applyConfig(dict) ─▶ queue ─▶ drain ─▶ KrkrzAndroidSetConfig(dict)         ← 一括通知
```

ポイント:

- 吉里吉里エンジンは**マルチスレッド非対応**。そのため native は Java からのリクエストを
  キューに積むだけで、実際の TJS 呼び出しは `continuous event hook`(= TJS メインスレッド)上の
  `drainRequests()` で行う。TJS の辞書生成・グローバル関数呼び出しもすべてこの中で完結する。
- グローバル関数は「**定義されていれば呼ぶ**」。未定義でもクラッシュせず、設定画面には
  「本体側に設定取得関数がありません」と表示されるだけ。
- 音量だけは「直接 get/set」。スライダーを動かすたびに `KrkrzAndroidSetVolume` が即時に
  呼ばれる (ライブ反映)。
- 「起動時に取得」は実際には**設定画面オープン時**に非同期取得する形にしている
  (アプリ起動直後は startup.tjs 未実行でグローバル未定義のため)。

---

## 2. タイトル側でやること (最短手順)

### Step 1. 参考実装を取り込む

`sample_config.tjs` の内容を、ゲームの起動スクリプト (`startup.tjs` など、System.* が使える
タイミング) から `Scripts.execStorage("...")` で読むか、内容をコピーして組み込む。
これだけで設定画面は「動く」(値はインメモリ辞書に保持されるだけ)。

```tjs
// startup.tjs などから
Scripts.execStorage("androidconfig_impl.tjs");   // sample_config.tjs をリネームして配置
```

### Step 2. 4 つのグローバル関数をゲームの設定変数へ配線する

`sample_config.tjs` の中の `// → 配線例` コメント箇所を、ゲームで実際に設定を保持している
変数・オブジェクトへの読み書きに差し替える。配線先の例:

| 役割              | 配線先の例 (KAG)                       | 配線先の例 (自前システム)         |
| ----------------- | -------------------------------------- | --------------------------------- |
| メッセージ速度    | `kag.drawSpeed` / `kag.userChSpeed`    | システム設定オブジェクトの該当値  |
| オート速度        | `kag.autoModeInterval`                 | 同上                              |
| スキップモード    | `kag.skipMode` / 既読管理フラグ        | 同上                              |
| 画面/システム効果 | エフェクト有効フラグ                   | 同上                              |
| 音量 (master 等)  | `WaveSoundBuffer.GlobalVolume` (0-100000) | サウンドマネージャのチャンネル音量 |
| 言語              | ゲームの言語切替変数                     | 同上                              |

音量は **0-100** で受け渡しする。`WaveSoundBuffer.GlobalVolume` は **0-100000** なので
`value * 1000` のようにスケールする (`sample_config.tjs` にコメントあり)。

### Step 3. 設定の永続化

`KrkrzAndroidSetConfig` / `KrkrzAndroidSetVolume` が呼ばれた後、ゲームのセーブ機構
(System の保存領域 / システムセーブデータ等) に保存して、次回起動時に
`KrkrzAndroidGetConfig` がその値を返すようにする。`sample_config.tjs` はインメモリ
保持のみなので、ここはタイトル側で追加する。

### Step 4. キャラ別ボイス (任意)

`KrkrzAndroidGetConfig()` が返す辞書の `charVoiceNames` に、改行区切りでキャラ名を入れると、
設定画面に「キャラ別ボイス音量」スライダーがその数だけ動的生成される。
各スライダーは音量キー `charVoice:<キャラ名>` で `GetVolume`/`SetVolume` される。

```tjs
global.KrkrzAndroidCharVoiceNames = "主人公\nヒロインA\nヒロインB";
```

空文字 (既定) ならキャラ別ボイスのセクションは出ない。

### Step 5. 言語の選択肢 (任意)

言語ドロップダウンの選択肢は `KrkrzAndroidGetConfig()` が返す
`mainTextLangList` / `subTextLangList` から作る。1 行 1 選択肢・改行区切り、各行は
`値<TAB>表示名`。TAB が無ければ値と表示名が同一。

```tjs
d["mainTextLangList"] = "ja\t日本語\nen\tEnglish\nzh-CN\t简体中文\nzh-TW\t繁體中文";
```

`*List` が無い / 空なら、現在値だけが選択肢として並ぶ。

---

## 3. キー対応表 (`コンフィグ項目.txt` ↔ 契約キー)

`KrkrzAndroidGetConfig()` / `SetConfig(dict)` で受け渡すフラット辞書のキー。
bool は整数 0/1。

| コンフィグ項目.txt           | キー                    | 型     | UI            |
| ---------------------------- | ----------------------- | ------ | ------------- |
| 画面エフェクト               | `screenEffect`          | bool   | スイッチ      |
| システムエフェクト           | `systemEffect`          | bool   | スイッチ      |
| ウインドウ 透明-不透明       | `windowOpacity`         | 0-100  | スライダー    |
| メッセージ速度               | `messageSpeed`          | 0-100  | スライダー    |
| オートモード速度             | `autoModeSpeed`         | 0-100  | スライダー    |
| 選択肢でオート解除           | `autoReleaseOnChoice`   | bool   | スイッチ      |
| オート時ボイス終了を待つ     | `autoWaitVoice`         | bool   | スイッチ      |
| スキップモード動作           | `skipMode`              | 0/1    | ドロップダウン (0=既読のみ / 1=すべて) |
| 選択肢でスキップ解除         | `skipReleaseOnChoice`   | bool   | スイッチ      |
| 既読文章は自動スキップ       | `skipReadText`          | bool   | スイッチ      |
| ボイス時に BGM を下げる      | `lowerBgmOnVoice`       | bool   | スイッチ      |
| ボイス時に SE を下げる       | `lowerSeOnVoice`        | bool   | スイッチ      |
| メインテキスト               | `mainTextLang`          | string | ドロップダウン |
| サブテキスト                 | `subTextLang`           | string | ドロップダウン |

メタデータ (取得時のみ。`SetConfig` には渡されない):
`charVoiceNames` / `mainTextLangList` / `subTextLangList`。

音量 (`KrkrzAndroidGetVolume` / `SetVolume`、0-100):

| コンフィグ項目.txt   | キー             |
| -------------------- | ---------------- |
| マスター音量         | `master`         |
| ＢＧＭ音量           | `bgm`            |
| ＳＥ音量             | `se`             |
| システムＳＥ音量     | `systemSe`       |
| ボイス音量           | `voice`          |
| システムボイス音量   | `systemVoice`    |
| キャラ別ボイス       | `charVoice:<名前>` |

---

## 4. 動作確認

1. `make build` (または `./gradlew :app:assembleDebug`) → `make install`。
2. ゲームを起動 → 左端スワイプ → 「機能設定」。
3. 仮想パッドの 2 スイッチの下に、本体コンフィグのセクションが出れば連携成功。
   - 「設定を読み込み中…」のまま / 「設定取得関数がありません」 → Step 1/2 が未完
     (グローバル関数が未定義 or 起動スクリプトで読み込めていない)。
4. 音量スライダーを動かして実際の音量が即変わるか (ライブ反映)。
5. 画面を閉じて再度開き、値が保持されているか (= `GetConfig`/永続化が機能しているか)。

ログタグ `androidconfig` を `adb logcat` で見ると、グローバル関数が見つからない等の
診断が出る (`make logcat`)。

---

## 5. トラブルシュート

| 症状 | 原因 / 対処 |
| ---- | ----------- |
| 「設定取得関数がありません」と出る | `KrkrzAndroidGetConfig` がグローバルに無い。起動スクリプトで `sample_config.tjs` 相当を読めているか確認 |
| セクションは出るが値が初期値のまま | `GetConfig` がゲームの実値を返していない (Step 2 未配線) |
| 音量を動かしても変わらない | `KrkrzAndroidSetVolume` 内の配線が未実装 (`// → 配線例` を埋める) |
| 再起動すると設定が戻る | 永続化が無い (Step 3) |
| キャラ別ボイスが出ない | `charVoiceNames` が空。改行区切りで名前を入れる |
| ビルドで `androidconfig` が組み込まれない | `app-config.json` の `cmake.plugins` / `cmake.staticPlugins` に `"androidconfig"` があるか。タイトル切替後は `make clean-cmake` |

---

## 6. 内部構造 (改修する人向け)

- `Main.cpp` — `tTVPContinuousEventCallbackIntf` を `TVPAddContinuousEventHook` で登録し、
  毎フレーム `androidconfig::drainRequests()` を呼ぶだけ。
- `ConfigBridge.cpp` —
  - JNI エントリ (`nativeRequestConfig` / `nativeApplyConfig` / `nativeSetVolume`) は
    UI スレッドで**キューに積むだけ**。`JNI_OnLoad` は playgames が同 `libkrkrz.so` 内で
    既に定義しているため定義せず、JavaVM は呼び出し時に `env->GetJavaVM` でキャッシュ。
  - `drainRequests()` (TJS スレッド) が `TVPGetScriptDispatch()` でグローバルを取得し、
    `PropGet` で関数の存在確認 → `FuncCall`。
  - TJS 辞書 → Java `HashMap` は json プラグインと同じ `tTJSDispatch` 派生 + `EnumMembers`
    で汎用列挙 (native は素のマーシャラ。キー知識は持たない)。
  - Java `Map` → TJS 辞書は `ncbPropAccessor` で構築。いずれも drain 内 = TJS スレッドで実行。
- Java 側 `jp.wamsoft.krkrz.config.KrkrzConfig` — 非同期取得 (`requestConfig` →
  `onConfigReady` を `Handler` で UI スレッドへ) とキャッシュ。
- `ui.SettingsActivity` — `config_container` に switch/slider/Spinner を実行時に動的生成。
  音量はライブ、その他は `onPause` で `applyConfig` 一括通知。

新しいコンフィグ項目を足すときは「native は変更不要」(汎用マーシャラのため)。
`SettingsActivity` の `build*Section` に行を足し、`manual.tjs` のキー表と
`sample_config.tjs` の配線を更新する。文字列は `values{,-ja,-zh-rCN,-zh-rTW}/strings.xml`
の 4 ファイルすべてに追加する。
