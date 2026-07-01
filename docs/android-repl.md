# Android REPL (adb 経由の TJS 対話シェル)

Debug ビルドの krkrz に、**adb 経由で TJS 式を評価できる REPL** を用意している。
PC の CLI が無い Android でも、abstract unix socket 経由でエンジンのメインスレッドに
TJS 式を投げて結果を受け取れる。デバッグ・状態観測・自動化に使う。

> エンジン側の REPL 全体像 (console / `-replfile` / 結果整形 / スレッド構造) は
> `krkrz_dev/src/core/doc/REPL.md` を参照。本書は **Android 固有の socket 経路** の話。

## 仕組み

- **コア流用**: 評価は既存の `ReplMainQueue` (トランスポート非依存・メインスレッド実行キュー)
  がそのまま担う。毎フレームの `TVPDrainREPL()` は SDL3 共通 `main.cpp` が呼ぶので、
  `KRKRZ_REPL` を ON にするだけで drain が付く。
- **socket フロントエンド**: `tTVPReplSocketChannel` (`common/utils/ReplSocketChannel.*`) が
  `tTVPReplFileChannel` の socket 版として追加されている。abstract namespace の
  Unix domain socket を listen し、「1 行受信 → `ReplMainQueue` で実行 → 1 行 JSON 応答」する。
- **有効化 (2 条件)**:
  1. **ビルド**: Debug ビルドのみ `KRKRZ_REPL=ON` (`app/src/main/cpp/CMakeLists.txt`)。
     Release には REPL 自体がコンパイルされない。icline (行編集) は NDK 非対応なので
     `KRKRZ_REPL_LINE_EDIT=OFF` で回避 (Android は console を使わず socket のみ)。
  2. **起動時**: `KrkrzActivity` が **debuggable ビルドのとき** `super.onCreate` 前に
     `Os.setenv("KRKRZ_REPL_SOCKET", getPackageName() + ".repl")` する。engine は
     `tTVPReplSocketChannel::ShouldStart()` でこの env (または CLI `-replsocket=<name>`)
     を見てチャネルを起動する。

### socket 名は案件ごとに一意 (パッケージ名由来)

socket 名は **`<applicationId>.repl`** (例: `jp.example.mygame.repl`) を自動採用する。
abstract socket 名はシステム全体で共有されるため、複数の krkrz アプリを入れても
衝突しないようパッケージ名から導いている。**案件側で手を入れる必要は無い** (app-config.json
の `applicationId` を変えれば socket 名も自動で追従する)。`make repl` も同じ規則で forward する。

## 使い方

いちばん簡単なのは `make repl` (転送 + 接続を一発):

```bash
make run      # Debug ビルドを起動 (REPL がこのとき listen を始める)
make repl     # tcp:9099 を localabstract:<applicationId>.repl へ forward して nc 接続
              # → TJS 式を 1 行入力して Enter、Ctrl+C で終了
```

`REPL_PORT=... make repl` でポート変更可 (既定 9099)。手動でやる場合:

```bash
adb forward tcp:9099 localabstract:<applicationId>.repl
nc 127.0.0.1 9099
```

例 (入力 → 応答):

```
1+2
{"ok":true,"result":"3","error":""}

System.exePath
{"ok":true,"result":"\"/data/.../\"","error":""}

Debug.message("hi")
{"ok":true,"result":"(void)","error":""}
```

応答 JSON: `{ "ok": bool, "result": "<pretty-printed>", "error": "<msg>" }`
(結果整形は `TVPPrettyPrint` = TJS の `Debug.prettyPrint`)。

### 高機能クライアント (Python) — `make repl-py`

`nc` の代わりに、**本家 console REPL に操作を寄せた** Python クライアント
`scripts/krkrz-repl.py` を用意している (行編集・履歴・自動複数行・外部エディタ・
色付き整形・自動 forward・自動再接続)。

```bash
make run       # Debug 起動
make repl-py   # forward してリッチクライアントで接続
```

単体でも使える (forward 済みなら port 指定、未 forward なら --socket で自分で forward):

```bash
python scripts/krkrz-repl.py --port 9099
python scripts/krkrz-repl.py --socket <applicationId>.repl   # adb forward も行う
```

**操作は本家にあわせてある**: プロンプト `krkrz>` / 継続 `...`、**括弧・クォートが
閉じるまで自動で継続入力**(複数行 = 1 コマンド)、`exit`/`quit`/Ctrl-D で終了。

engine 側 dot コマンド (nc でも使える。socket チャネルが解釈):

| コマンド | 説明 |
|---|---|
| `.help` | engine 側ヘルプ |
| `.depth [N]` | 結果表示の展開深さを表示 / 設定 |
| `.compact [on\|off]` | 結果表示のコンパクト表示を表示 / 切替 |

クライアント側コマンド:

| コマンド | 説明 |
|---|---|
| `exit` / `quit` / Ctrl-D | 終了 (アプリは終了しない) |
| `.clear` | 入力中の継続バッファを破棄 |
| `.reconnect` / `.rc` | 接続を張り直す (アプリ再起動後など) |
| `.edit` / `.e` | `$EDITOR` を開き、内容を 1 コマンドとして送信 |
| `.load <file>` / `.l` | ファイルを 1 コマンドとして送信 |
| `.info` | 接続情報 |
| `.clienthelp` | クライアント側ヘルプ |

依存は標準ライブラリのみ。`pip install prompt_toolkit` を入れると Windows でも
インライン行編集が快適になる (無くても readline / 素の input にフォールバック)。

> 本家との差異: `exit`/`quit` は本家ではアプリ終了だが、リモート接続では**接続を
> 閉じるだけ**(アプリは動作継続)。`.mem`/`.cap`/`.dlg` 等の Agent/メモリ系は console
> /Elements 依存で socket では非対応。

## 注意・制限

- **Debug ビルド専用**。Release には REPL が無い (セキュリティ・サイズ)。
- 複数行入力は括弧・クォートの釣り合いで自動継続 (engine へは行末 `\` 継続で改行を保って
  送る)。nc から手打ちする場合も行末 `\` で複数行を 1 コマンドにできる。
- 評価は**メインスレッドで直列実行**される (TJS のスレッドアフィニティを守るため)。
  重い式はゲーム進行を一瞬止める。
- **`Agent.*` (入力注入・画面キャプチャ・ダイアログ操作) は Android では未対応**。
  Agent は Elements 依存で、Android は `KRKRZ_USE_ELEMENTS=OFF` のため。基本の TJS 式
  評価・グローバル/プラグイン API 参照は使える。
- 同時接続は 1 クライアント。切断すると次の接続を受け付ける。
- socket 名は `<applicationId>.repl` 固定規則で自動導出 (案件で意識不要)。`make repl` は
  `app-config.json` の `applicationId` から同じ名前を組み立てて forward する。

## 関連コード

- エンジン: `krkrz_dev/src/core/common/utils/ReplSocketChannel.{h,cpp}` (新規),
  `REPL.cpp` (`TVPCreateREPL`/`TVPDestroyREPL` への配線), `sources.cmake` (`KRKRZ_REPL` 群)。
- Android: `app/src/main/cpp/CMakeLists.txt` (Debug で `KRKRZ_REPL=ON`),
  `KrkrzActivity.java` (debuggable 時に `KRKRZ_REPL_SOCKET` を設定)。
