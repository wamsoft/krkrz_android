#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
krkrz-repl.py — Android 実機の krkrz REPL に繋ぐ対話クライアント (nc の高機能版)。

操作仕様は本家 krkrz 内蔵 REPL (console, doc/REPL.md) にできるだけ寄せてある:
  - プロンプト `krkrz>` / 継続は `...`
  - 括弧・クォートが閉じるまで自動で継続入力になる (複数行 = 1 コマンド)
  - `exit` / `quit` / Ctrl-D で終了
  - `.help` `.depth [N]` `.compact [on|off]` などの dot コマンド
engine 側 socket チャネルが「1 行 TJS → 1 行 JSON({ok,result,error})」を返し、
行末 `\\` の継続と `.depth/.compact/.help` を解釈する。本クライアントはさらに
行編集・履歴・外部エディタ・色付き整形・自動 forward・自動再接続を足している。

使い方:
    python scripts/krkrz-repl.py --port 9099            # forward 済みならこれだけ
    python scripts/krkrz-repl.py --socket <appId>.repl  # adb forward も行う
    make repl-py                                          # Makefile 経由 (APP_ID 自動)

依存: 標準ライブラリのみ。prompt_toolkit があれば inline 編集が快適 (Windows 含む)。
"""
import argparse
import atexit
import json
import os
import socket
import subprocess
import sys
import tempfile

HISTFILE = os.path.expanduser("~/.krkrz_history")

# ---- 入力バックエンド (prompt_toolkit > readline > input) ----------------------
_pt_session = None
def _init_input():
    global _pt_session
    try:
        from prompt_toolkit import PromptSession
        from prompt_toolkit.history import FileHistory
        _pt_session = PromptSession(history=FileHistory(HISTFILE))
        return
    except Exception:
        pass
    try:
        import readline
        try:
            readline.read_history_file(HISTFILE)
        except Exception:
            pass
        atexit.register(lambda: _safe(readline.write_history_file, HISTFILE))
    except Exception:
        pass

def _safe(fn, *a):
    try:
        fn(*a)
    except Exception:
        pass

def read_line(prompt):
    if _pt_session is not None:
        return _pt_session.prompt(prompt)
    return input(prompt)

# ---- 色付け -------------------------------------------------------------------
class C:
    enabled = sys.stdout.isatty()
def _c(code, s):
    return s if not C.enabled else f"\033[{code}m{s}\033[0m"
def green(s): return _c("32", s)
def red(s):   return _c("31", s)
def gray(s):  return _c("90", s)
def cyan(s):  return _c("36", s)

# ---- TJS 文が完結しているか (括弧・クォート・コメントの釣り合い) ----------------
def is_complete(text):
    depth = 0
    instr = None          # None / '"' / "'"
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if instr is not None:
            if c == '\\':
                i += 2; continue
            if c == instr:
                instr = None
            i += 1; continue
        if c in '"\'':
            instr = c; i += 1; continue
        if c == '/' and i + 1 < n and text[i + 1] == '/':
            j = text.find('\n', i)
            if j == -1:
                break
            i = j; continue
        if c == '/' and i + 1 < n and text[i + 1] == '*':
            j = text.find('*/', i + 2)
            if j == -1:
                return False           # 未終端のブロックコメント → 継続
            i = j + 2; continue
        if c in '([{':
            depth += 1
        elif c in ')]}':
            depth -= 1
        i += 1
    return instr is None and depth <= 0

# ---- REPL クライアント本体 -----------------------------------------------------
class ReplClient:
    def __init__(self, host, port):
        self.host, self.port = host, port
        self.sock = None
        self.buf = b""

    def connect(self):
        self.close()
        self.sock = socket.create_connection((self.host, self.port), timeout=10)
        self.sock.settimeout(None)
        self.buf = b""

    def close(self):
        if self.sock:
            _safe(self.sock.close)
            self.sock = None

    def _read_line(self):
        while b"\n" not in self.buf:
            chunk = self.sock.recv(65536)
            if not chunk:
                raise ConnectionError("connection closed by peer")
            self.buf += chunk
        line, _, self.buf = self.buf.partition(b"\n")
        return line.decode("utf-8", "replace")

    def eval_lines(self, lines):
        """物理行のリストを 1 コマンドとして送る。
        改行を保つため、最終行以外の行末に '\\' を付けて送信 (engine が継続として復元)。"""
        if self.sock is None:
            self.connect()
        wire = "\\\n".join(lines) + "\n"
        self.sock.sendall(wire.encode("utf-8"))
        raw = self._read_line()
        try:
            return json.loads(raw)
        except Exception:
            return {"ok": False, "result": "", "error": f"(bad JSON) {raw}"}

# ---- 応答表示 -----------------------------------------------------------------
def show_response(resp):
    if resp.get("ok"):
        result = resp.get("result", "")
        print(green(result) if result else gray("(void)"))
    else:
        print(red("! " + (resp.get("error", "") or "(unknown error)")))

def send(client, lines):
    if not lines or not "".join(lines).strip():
        return
    try:
        show_response(client.eval_lines(lines))
    except (ConnectionError, OSError) as e:
        print(red(f"! disconnected: {e}   (.reconnect で張り直し)"))
        client.close()

# ---- 外部エディタ --------------------------------------------------------------
def run_editor():
    editor = os.environ.get("EDITOR") or ("notepad" if os.name == "nt" else "vi")
    with tempfile.NamedTemporaryFile("w+", suffix=".tjs", delete=False, encoding="utf-8") as tf:
        path = tf.name
        tf.write("// krkrz REPL: 保存して閉じると 1 コマンドとして送信されます。\n")
    try:
        subprocess.call(editor.split() + [path])
        with open(path, "r", encoding="utf-8") as f:
            return [ln for ln in f.read().splitlines() if not ln.lstrip().startswith("//")]
    finally:
        _safe(os.unlink, path)

HELP = """\
krkrz REPL クライアント — 操作は本家 (console REPL) にあわせています。

  式や文を入力して Enter で評価。括弧・クォートが閉じるまで継続入力 (... プロンプト)。
  複数文はセミコロンで繋ぐか、複数行のまま入力できます。

engine 側 dot コマンド (nc でも使用可):
  .help              engine 側ヘルプ
  .depth [N]         結果表示の展開深さを表示/設定
  .compact [on|off]  結果表示のコンパクト表示を表示/切替

クライアント側コマンド:
  exit / quit / Ctrl-D   終了 (アプリは終了しない)
  .clear                 入力中の継続バッファを破棄
  .reconnect / .rc       接続を張り直す (アプリ再起動後など)
  .edit / .e             $EDITOR を開き、書いた内容を 1 コマンドとして送信
  .load <file> / .l      ファイルを 1 コマンドとして送信
  .info                  接続情報
  .clienthelp            このヘルプ"""

# クライアント側で横取りする meta (これ以外の .xxx は engine へ転送)。
def repl_loop(client):
    print(cyan(f"krkrz REPL  ({client.host}:{client.port})  —  .clienthelp / exit / Ctrl-D"))
    pending = []
    while True:
        prompt = "krkrz> " if not pending else "... "
        try:
            line = read_line(prompt)
        except EOFError:
            print(); break
        except KeyboardInterrupt:
            pending = []; print("^C"); continue

        s = line.strip()

        # 継続中でも効く meta
        if s == ".clear":
            pending = []; continue

        if not pending:
            # 文の途中でない時だけ meta / 単発 dot を解釈
            if s in ("exit", "quit", ".quit", ".q"):
                break
            if s in (".clienthelp",):
                print(HELP); continue
            if s in (".reconnect", ".rc"):
                try:
                    client.connect(); print(gray("reconnected."))
                except Exception as e:
                    print(red(f"! reconnect failed: {e}"))
                continue
            if s in (".info",):
                print(gray(f"host={client.host} port={client.port} "
                           f"connected={'yes' if client.sock else 'no'}"))
                continue
            if s in (".edit", ".e"):
                send(client, run_editor()); continue
            if s.startswith(".load ") or s.startswith(".l "):
                path = os.path.expanduser(s.split(None, 1)[1].strip())
                try:
                    with open(path, "r", encoding="utf-8") as f:
                        send(client, f.read().splitlines())
                except OSError as e:
                    print(red(f"! {e}"))
                continue

        # 通常入力: 括弧・クォートが閉じるまで継続
        pending.append(line)
        joined = "\n".join(pending)
        if is_complete(joined):
            lines = pending
            pending = []
            if joined.strip():
                send(client, lines)

def main():
    ap = argparse.ArgumentParser(description="krkrz Android REPL client")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=9099)
    ap.add_argument("--socket", help="abstract socket 名 (指定すると adb forward も行う, 例 <applicationId>.repl)")
    ap.add_argument("--adb", default="adb", help="adb 実行パス (--socket 使用時)")
    ap.add_argument("--no-color", action="store_true")
    args = ap.parse_args()

    if args.no_color:
        C.enabled = False

    if args.socket:
        try:
            subprocess.run([args.adb, "forward", f"tcp:{args.port}",
                            f"localabstract:{args.socket}"], check=True)
            print(gray(f"adb forward tcp:{args.port} -> localabstract:{args.socket}"))
        except (subprocess.CalledProcessError, FileNotFoundError) as e:
            print(red(f"! adb forward failed: {e}")); return 1

    _init_input()
    client = ReplClient(args.host, args.port)
    try:
        client.connect()
    except OSError as e:
        print(red(f"! connect {args.host}:{args.port} failed: {e}"))
        print(gray("  Debug ビルドで起動中か、forward 済みか確認 (make run / make repl / --socket)"))
        return 1
    try:
        repl_loop(client)
    finally:
        client.close()
    return 0

if __name__ == "__main__":
    sys.exit(main())
