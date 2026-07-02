#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Google Play Console 公開自動化 (Play Developer API v3 / androidpublisher 直叩き)。

コンソールを開かずに以下を CLI / make から行う:
  status       各トラックの現状 (versionCode / 状態 / 段階公開率 / リリースノート) を照会 (読み取り専用)
  upload       release AAB をアップロードしトラックに割り当て (+ リリースノート添付)
  promote      既存 versionCode を別トラックへ昇格 (再アップロード不要)
  rollout      段階公開率の増減 / 完了 / 停止
  notes-new    サポート全言語のリリースノート雛形を一括生成 (API 不要・オフライン可)
  notes-check  リリースノートが全言語揃っているか + 各 500 文字以内かを検査 (API 不要)

設定の解決 (gradle / Makefile と同じ規則):
  PROJECT_DIR       案件フォルダ (未設定なら BUILD_SYSTEM_DIR)
  APP_CONFIG_FILE   app-config.json の位置 (local.properties > 環境変数 > PROJECT_DIR/app-config.json)
  applicationId     app-config.json から取得 = Play の packageName
  サービスアカウント鍵  local.properties の PLAY_SERVICE_ACCOUNT_JSON または環境変数 (JSON 鍵ファイルのパス)

リリースノート: ${PROJECT_DIR}/dist/playstore/whatsnew/<versionCode>/<locale>.txt (+ default.txt フォールバック)
  <locale> は Play の言語コード (en-US / ja-JP / zh-CN / zh-TW ...)。アプリ res の values-* から導出。

詳細: docs/android-play-publishing.md
"""

import argparse
import os
import re
import sys

# --- リポジトリ / 案件パス解決 -------------------------------------------------

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BUILD_SYSTEM_DIR = os.environ.get("BUILD_SYSTEM_DIR") or os.path.dirname(SCRIPT_DIR)
PROJECT_DIR = os.environ.get("PROJECT_DIR") or BUILD_SYSTEM_DIR
BUILD_ROOT = os.path.join(PROJECT_DIR, "build", "android")


def die(msg):
    print("ERROR: " + msg, file=sys.stderr)
    sys.exit(1)


def read_local_properties():
    path = os.path.join(BUILD_SYSTEM_DIR, "local.properties")
    props = {}
    if os.path.isfile(path):
        with open(path, encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#") or "=" not in line:
                    continue
                k, v = line.split("=", 1)
                props[k.strip()] = v.strip()
    return props


LOCAL_PROPS = read_local_properties()


def expand_vars(s):
    """${BUILD_SYSTEM_DIR} / ${PROJECT_DIR} / ${ENV_VAR} を展開 (gradle と同じ)。"""
    if s is None:
        return None

    def repl(m):
        name = m.group(1)
        if name == "BUILD_SYSTEM_DIR":
            return BUILD_SYSTEM_DIR
        if name == "PROJECT_DIR":
            return PROJECT_DIR
        return os.environ.get(name, "")

    return re.sub(r"\$\{([A-Za-z_][A-Za-z0-9_]*)\}", repl, s)


def resolve_path(s):
    """${...} 展開後、相対パスは PROJECT_DIR 基準で解決。"""
    expanded = expand_vars(s)
    if not expanded:
        return None
    expanded = expanded.replace("\\", "/")
    if os.path.isabs(expanded):
        return expanded
    return os.path.join(PROJECT_DIR, expanded)


def resolve_app_config_file():
    cand = LOCAL_PROPS.get("APP_CONFIG_FILE") or os.environ.get("APP_CONFIG_FILE")
    if cand:
        p = resolve_path(cand)
        if not p or not os.path.isfile(p):
            die("APP_CONFIG_FILE が存在しません: {} (from '{}')".format(p, cand))
        return p
    p = os.path.join(PROJECT_DIR, "app-config.json")
    if not os.path.isfile(p):
        die("app-config.json が見つかりません: {}. PROJECT_DIR を設定してください。".format(p))
    return p


def load_app_config():
    import json

    with open(resolve_app_config_file(), encoding="utf-8") as f:
        return json.load(f)


APP_CONFIG = load_app_config()


def package_name():
    pkg = APP_CONFIG.get("applicationId")
    if not pkg:
        die("app-config.json に applicationId がありません。")
    return pkg


def config_version_code():
    return int(APP_CONFIG.get("versionCode", 0))


# --- リリースノート (locale 導出 + 読み書き) ----------------------------------

# 既知の言語 → 既定リージョン (Play の言語コードは lang-REGION 形式が多い)
_DEFAULT_REGION = {
    "en": "US", "ja": "JP", "ko": "KR", "de": "DE", "fr": "FR",
    "es": "ES", "it": "IT", "pt": "PT", "ru": "RU", "th": "TH",
    "vi": "VN", "id": "ID", "tr": "TR", "pl": "PL", "nl": "NL",
}


def qualifier_to_bcp47(q):
    """Android res の values 修飾子 → Play 言語コード。ロケール以外 (night/v26 等) は None。

    ''            -> en-US  (このテンプレートの既定言語は英語)
    'ja'          -> ja-JP
    'zh-rCN'      -> zh-CN
    'pt-rBR'      -> pt-BR
    """
    if q == "":
        return "en-US"
    m = re.match(r"^([a-z]{2,3})(?:-r([A-Za-z]{2,3}))?$", q)
    if not m:
        return None
    lang, region = m.group(1), m.group(2)
    if region:
        return "{}-{}".format(lang, region.upper())
    if lang in _DEFAULT_REGION:
        return "{}-{}".format(lang, _DEFAULT_REGION[lang])
    return lang


def res_dirs():
    """走査対象の res ディレクトリ: ビルドシステム側 + 案件の resOverride。"""
    dirs = [os.path.join(BUILD_SYSTEM_DIR, "app", "src", "main", "res")]
    ap = APP_CONFIG.get("assetPack") or {}
    ro = ap.get("resOverride")
    if ro:
        names = ro if isinstance(ro, list) else [ro]
        for n in names:
            p = resolve_path(n)
            if p and os.path.isdir(p):
                dirs.append(p)
    return [d for d in dirs if os.path.isdir(d)]


def supported_locales():
    """res の values-* からサポート言語 (Play 言語コード) を導出。順序安定・重複排除。"""
    seen = {}
    for d in res_dirs():
        for name in sorted(os.listdir(d)):
            if not os.path.isdir(os.path.join(d, name)):
                continue
            if name == "values":
                q = ""
            elif name.startswith("values-"):
                q = name[len("values-"):]
            else:
                continue
            code = qualifier_to_bcp47(q)
            if code:
                seen[code] = True
    # en-US を先頭に、残りはアルファベット順
    codes = list(seen.keys())
    codes.sort(key=lambda c: (c != "en-US", c))
    return codes


def whatsnew_dir(version_code):
    return os.path.join(PROJECT_DIR, "dist", "playstore", "whatsnew", str(version_code))


def load_release_notes(version_code):
    """<locale>.txt を読み、[{language, text}] を返す。無ければ default.txt をフォールバック。"""
    d = whatsnew_dir(version_code)
    if not os.path.isdir(d):
        return []
    default_path = os.path.join(d, "default.txt")
    default_text = None
    if os.path.isfile(default_path):
        with open(default_path, encoding="utf-8") as f:
            default_text = f.read().strip()
    notes = []
    for code in supported_locales():
        p = os.path.join(d, code + ".txt")
        text = None
        if os.path.isfile(p):
            with open(p, encoding="utf-8") as f:
                text = f.read().strip()
        elif default_text is not None:
            text = default_text
        if text:
            if len(text) > 500:
                die("リリースノートが 500 文字超過: {} ({} 文字)".format(p, len(text)))
            notes.append({"language": code, "text": text})
    return notes


# --- API クライアント ----------------------------------------------------------

SCOPES = ["https://www.googleapis.com/auth/androidpublisher"]


def _cfg(key):
    """local.properties > 環境変数 の順で設定値を取得。"""
    return LOCAL_PROPS.get(key) or os.environ.get(key)


def _resolve_existing(cand, what):
    p = resolve_path(cand) if ("${" in cand or not os.path.isabs(cand.replace("\\", "/"))) \
        else cand.replace("\\", "/")
    if not p or not os.path.isfile(p):
        die("{} が見つかりません: {}".format(what, p))
    return p


def oauth_token_path():
    """OAuth の refresh token キャッシュ先 (gitignore 済)。"""
    cand = _cfg("PLAY_OAUTH_TOKEN")
    if cand:
        return resolve_path(cand)
    return os.path.join(BUILD_SYSTEM_DIR, ".play-oauth-token.json")


def _save_token(creds, path):
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write(creds.to_json())
    print("  OAuth トークンを保存: {}".format(path))


def _oauth_credentials(client_json):
    """OAuth ユーザー認可 (ローカル向け): 初回はブラウザで同意 → refresh token を再利用。"""
    try:
        from google.oauth2.credentials import Credentials
        from google.auth.transport.requests import Request
    except ImportError:
        die("依存パッケージが未インストールです:\n  python -m pip install -r scripts/requirements-play.txt")

    token_path = oauth_token_path()
    creds = None
    if os.path.isfile(token_path):
        creds = Credentials.from_authorized_user_file(token_path, SCOPES)
    if creds and creds.valid:
        return creds
    if creds and creds.expired and creds.refresh_token:
        try:
            creds.refresh(Request())
            _save_token(creds, token_path)
            return creds
        except Exception:
            creds = None  # 失効/取消 → 再同意へ

    try:
        from google_auth_oauthlib.flow import InstalledAppFlow
    except ImportError:
        die("OAuth 方式には google-auth-oauthlib が必要です:\n"
            "  python -m pip install -r scripts/requirements-play.txt")
    print("ブラウザで Google 認可を行います (Play Console 権限を持つアカウントでログイン)...")
    flow = InstalledAppFlow.from_client_secrets_file(client_json, SCOPES)
    creds = flow.run_local_server(port=0)
    _save_token(creds, token_path)
    return creds


def get_credentials():
    """認証情報を解決。CI=サービスアカウント / ローカル=OAuth を自動判定 (PLAY_AUTH で強制可)。"""
    method = (_cfg("PLAY_AUTH") or "").strip().lower()
    sa = _cfg("PLAY_SERVICE_ACCOUNT_JSON")
    oc = _cfg("PLAY_OAUTH_CLIENT_JSON")
    if not method:
        # 両方あれば CI 向けのサービスアカウントを優先
        method = "service" if sa else ("oauth" if oc else "")

    if method == "service":
        if not sa:
            die("PLAY_SERVICE_ACCOUNT_JSON が未設定です (PLAY_AUTH=service 指定時)。")
        try:
            from google.oauth2 import service_account
        except ImportError:
            die("依存パッケージが未インストールです:\n  python -m pip install -r scripts/requirements-play.txt")
        return service_account.Credentials.from_service_account_file(
            _resolve_existing(sa, "サービスアカウント鍵"), scopes=SCOPES)

    if method == "oauth":
        if not oc:
            die("PLAY_OAUTH_CLIENT_JSON が未設定です (PLAY_AUTH=oauth 指定時)。")
        return _oauth_credentials(_resolve_existing(oc, "OAuth クライアント JSON"))

    die("認証情報が未設定です。local.properties に次のいずれかを設定してください:\n"
        "  PLAY_SERVICE_ACCOUNT_JSON=...   (CI / 無人向け: サービスアカウント鍵)\n"
        "  PLAY_OAUTH_CLIENT_JSON=...      (ローカル向け: OAuth クライアント JSON、初回ブラウザ同意)\n"
        "詳細: docs/android-play-publishing.md")


def build_service():
    try:
        from googleapiclient.discovery import build
        import httplib2
        import google_auth_httplib2
    except ImportError:
        die("依存パッケージが未インストールです。次を実行してください:\n"
            "  python -m pip install -r scripts/requirements-play.txt")
    # 数 GB の AAB (base + PAD asset packs) アップロードで httplib2 の既定タイムアウトに
    # 引っかかるため、ソケットタイムアウトを延長する (PLAY_HTTP_TIMEOUT 秒, 既定 600)。
    timeout = int(_cfg("PLAY_HTTP_TIMEOUT") or "600")
    http_obj = httplib2.Http(timeout=timeout)
    # resumable upload のチャンク応答 308 "Resume Incomplete" を httplib2 が redirect と
    # 誤認し、Location ヘッダ無しで RedirectMissingLocation を投げる問題を回避。
    # 追従を無効化すると 308 がそのまま googleapiclient の next_chunk に渡り正しく継続される。
    http_obj.follow_redirects = False
    authed_http = google_auth_httplib2.AuthorizedHttp(get_credentials(), http=http_obj)
    return build("androidpublisher", "v3", http=authed_http, cache_discovery=False)


class Edit:
    """edits.insert -> ... -> commit/abandon を with で扱うヘルパー。"""

    def __init__(self, service, pkg, commit=True):
        self.service = service
        self.pkg = pkg
        self.edits = service.edits()
        self.commit = commit
        self.id = None

    def __enter__(self):
        self.id = self.edits.insert(packageName=self.pkg, body={}).execute()["id"]
        return self

    def __exit__(self, exc_type, exc, tb):
        if exc_type is not None:
            # 失敗時は破棄
            try:
                self.edits.delete(packageName=self.pkg, editId=self.id).execute()
            except Exception:
                pass
            return False
        if self.commit:
            self.edits.validate(packageName=self.pkg, editId=self.id).execute()
            self.edits.commit(packageName=self.pkg, editId=self.id).execute()
        else:
            # 読み取り専用 / dry-run: validate だけして破棄
            try:
                self.edits.validate(packageName=self.pkg, editId=self.id).execute()
            except Exception:
                pass
            self.edits.delete(packageName=self.pkg, editId=self.id).execute()
        return False


# --- サブコマンド --------------------------------------------------------------

def cmd_status(args):
    svc = build_service()
    pkg = package_name()
    print("package: {}".format(pkg))
    with Edit(svc, pkg, commit=False) as e:
        tracks = svc.edits().tracks().list(packageName=pkg, editId=e.id).execute()
        items = tracks.get("tracks", [])
        if not items:
            print("(トラックにリリースはありません)")
            return
        for t in items:
            print("\n== track: {} ==".format(t.get("track")))
            for r in t.get("releases", []):
                vcs = ",".join(str(v) for v in r.get("versionCodes", []) or [])
                line = "  - status={} versionCodes=[{}]".format(r.get("status"), vcs)
                if r.get("name"):
                    line += " name={}".format(r["name"])
                if r.get("userFraction") is not None:
                    line += " userFraction={}".format(r["userFraction"])
                print(line)
                for n in r.get("releaseNotes", []) or []:
                    txt = (n.get("text") or "").replace("\n", " ")
                    if len(txt) > 60:
                        txt = txt[:60] + "…"
                    print("      [{}] {}".format(n.get("language"), txt))


def _aab_path(args):
    if args.aab:
        return args.aab
    p = os.path.join(BUILD_ROOT, "app", "outputs", "bundle", "release", "app-release.aab")
    if not os.path.isfile(p):
        die("release AAB が見つかりません: {}\n先に `make pad-aab` (PAD案件) または `./gradlew bundleRelease` を実行してください。".format(p))
    return p


def _release_body(status, version_codes, rollout, notes):
    body = {"status": status, "versionCodes": [int(v) for v in version_codes]}
    if rollout is not None:
        body["userFraction"] = float(rollout)
    if notes:
        body["releaseNotes"] = notes
    return body


def cmd_upload(args):
    from googleapiclient.http import MediaFileUpload

    aab = _aab_path(args)
    svc = build_service()
    pkg = package_name()

    status = args.status
    rollout = None
    if args.rollout is not None:
        rollout = float(args.rollout)
        if 0.0 < rollout < 1.0:
            status = "inProgress"
        elif rollout >= 1.0:
            status, rollout = "completed", None

    print("upload: {} -> track '{}' (status={}{})".format(
        os.path.basename(aab), args.track, status,
        ", rollout={}".format(rollout) if rollout is not None else ""))

    print("  AAB サイズ: {:.1f} MB (数 GB の場合アップロードに数分かかります)".format(
        os.path.getsize(aab) / (1024 * 1024)))
    with Edit(svc, pkg, commit=not args.dry_run) as e:
        media = MediaFileUpload(aab, mimetype="application/octet-stream",
                                chunksize=8 * 1024 * 1024, resumable=True)
        req = svc.edits().bundles().upload(packageName=pkg, editId=e.id, media_body=media)
        resp = None
        while resp is None:
            # num_retries: 一時的な 5xx / ネットワーク断は指数バックオフで自動再送。
            st, resp = req.next_chunk(num_retries=5)
            if st:
                print("  uploading... {}%".format(int(st.progress() * 100)))
        vc = resp["versionCode"]
        print("  uploaded versionCode={}".format(vc))

        notes = load_release_notes(vc)
        if notes:
            print("  release notes: {}".format(", ".join(n["language"] for n in notes)))
        else:
            print("  release notes: (無し — dist/playstore/whatsnew/{}/ が空)".format(vc))

        body = {"track": args.track, "releases": [_release_body(status, [vc], rollout, notes)]}
        svc.edits().tracks().update(packageName=pkg, editId=e.id, track=args.track, body=body).execute()

    print("完了。" + ("(dry-run: commit していません)" if args.dry_run else
                     "track '{}' に versionCode={} を割り当てました。".format(args.track, vc)))


def cmd_promote(args):
    svc = build_service()
    pkg = package_name()
    status = args.status
    rollout = None
    if args.rollout is not None:
        rollout = float(args.rollout)
        if 0.0 < rollout < 1.0:
            status = "inProgress"
        elif rollout >= 1.0:
            status, rollout = "completed", None

    with Edit(svc, pkg, commit=not args.dry_run) as e:
        src = svc.edits().tracks().get(packageName=pkg, editId=e.id, track=args.src).execute()
        vcs = []
        for r in src.get("releases", []):
            vcs += [int(v) for v in r.get("versionCodes", []) or []]
        if not vcs:
            die("昇格元トラック '{}' にリリースがありません。".format(args.src))
        vc = max(vcs)
        print("promote: versionCode={} を '{}' -> '{}' (status={}{})".format(
            vc, args.src, args.dst, status,
            ", rollout={}".format(rollout) if rollout is not None else ""))
        notes = load_release_notes(vc)
        body = {"track": args.dst, "releases": [_release_body(status, [vc], rollout, notes)]}
        svc.edits().tracks().update(packageName=pkg, editId=e.id, track=args.dst, body=body).execute()

    print("完了。" + ("(dry-run: commit していません)" if args.dry_run else
                     "track '{}' に versionCode={} を昇格しました。".format(args.dst, vc)))


def cmd_rollout(args):
    svc = build_service()
    pkg = package_name()
    with Edit(svc, pkg, commit=not args.dry_run) as e:
        t = svc.edits().tracks().get(packageName=pkg, editId=e.id, track=args.track).execute()
        releases = t.get("releases", [])
        target = None
        for r in releases:
            if r.get("status") == "inProgress":
                target = r
                break
        if target is None:
            die("track '{}' に段階公開中 (inProgress) のリリースがありません。".format(args.track))

        if args.halt:
            target["status"] = "halted"
            target.pop("userFraction", None)
            print("rollout: track '{}' を停止 (halted) します。".format(args.track))
        else:
            f = float(args.fraction)
            if f >= 1.0:
                target["status"] = "completed"
                target.pop("userFraction", None)
                print("rollout: track '{}' を全公開 (completed) にします。".format(args.track))
            else:
                target["status"] = "inProgress"
                target["userFraction"] = f
                print("rollout: track '{}' の公開率を {} にします。".format(args.track, f))

        svc.edits().tracks().update(packageName=pkg, editId=e.id, track=args.track,
                                    body={"track": args.track, "releases": releases}).execute()
    print("完了。" + ("(dry-run: commit していません)" if args.dry_run else ""))


def cmd_notes_new(args):
    vc = args.version or config_version_code()
    if not vc:
        die("versionCode を特定できません。--version N を指定してください。")
    d = whatsnew_dir(vc)
    os.makedirs(d, exist_ok=True)
    locales = supported_locales()
    print("versionCode={} のリリースノート雛形を生成: {}".format(vc, d))
    print("サポート言語 (res の values-* から導出): {}".format(", ".join(locales)))
    created, skipped = 0, 0
    for code in locales:
        p = os.path.join(d, code + ".txt")
        if os.path.isfile(p):
            skipped += 1
            continue
        with open(p, "w", encoding="utf-8", newline="\n") as f:
            f.write("")  # 空ファイル (500 文字以内で記入)
        created += 1
    # default.txt (未記入言語のフォールバック) も用意
    dp = os.path.join(d, "default.txt")
    if not os.path.isfile(dp):
        with open(dp, "w", encoding="utf-8", newline="\n") as f:
            f.write("")
    print("  作成 {} 件 / 既存 {} 件 (+ default.txt)。各ファイルに 500 文字以内で記入してください。".format(created, skipped))
    print("  下書きに git/svn ログを使う例は docs/android-play-publishing.md を参照。")


def cmd_notes_check(args):
    vc = args.version or config_version_code()
    if not vc:
        die("versionCode を特定できません。--version N を指定してください。")
    d = whatsnew_dir(vc)
    if not os.path.isdir(d):
        die("リリースノートフォルダがありません: {} (先に notes-new を実行)".format(d))
    locales = supported_locales()
    default_path = os.path.join(d, "default.txt")
    has_default = os.path.isfile(default_path) and open(default_path, encoding="utf-8").read().strip()
    ok = True
    print("versionCode={} リリースノート検査 ({})".format(vc, d))
    for code in locales:
        p = os.path.join(d, code + ".txt")
        text = ""
        if os.path.isfile(p):
            with open(p, encoding="utf-8") as f:
                text = f.read().strip()
        if not text:
            if has_default:
                print("  [{}] 空 → default.txt でフォールバック".format(code))
            else:
                print("  [{}] ✗ 空 (かつ default.txt も無し)".format(code))
                ok = False
        elif len(text) > 500:
            print("  [{}] ✗ 500 文字超過 ({} 文字)".format(code, len(text)))
            ok = False
        else:
            print("  [{}] ✓ {} 文字".format(code, len(text)))
    if not ok:
        die("未記入 / 文字数超過の言語があります。")
    print("OK: 全言語そろっています。")


# --- argparse ------------------------------------------------------------------

def main():
    p = argparse.ArgumentParser(description="Google Play Console 公開自動化")
    sub = p.add_subparsers(dest="cmd", required=True)

    sp = sub.add_parser("status", help="各トラックの現状を照会 (読み取り専用)")
    sp.set_defaults(func=cmd_status)

    sp = sub.add_parser("upload", help="release AAB をアップロードしトラックへ割り当て")
    sp.add_argument("--track", default="internal")
    sp.add_argument("--status", default="completed",
                    choices=["completed", "draft", "inProgress", "halted"])
    sp.add_argument("--rollout", default=None, help="段階公開率 0<f<1 (指定で status=inProgress)")
    sp.add_argument("--aab", default=None, help="AAB パス (既定: build/android/app/outputs/bundle/release/app-release.aab)")
    sp.add_argument("--dry-run", action="store_true", help="validate のみで commit しない")
    sp.set_defaults(func=cmd_upload)

    sp = sub.add_parser("promote", help="既存 versionCode を別トラックへ昇格 (再アップロード不要)")
    sp.add_argument("--from", dest="src", required=True)
    sp.add_argument("--to", dest="dst", required=True)
    sp.add_argument("--status", default="completed",
                    choices=["completed", "draft", "inProgress", "halted"])
    sp.add_argument("--rollout", default=None, help="段階公開率 0<f<1 (指定で status=inProgress)")
    sp.add_argument("--dry-run", action="store_true")
    sp.set_defaults(func=cmd_promote)

    sp = sub.add_parser("rollout", help="段階公開率の増減 / 完了 / 停止")
    sp.add_argument("--track", default="production")
    sp.add_argument("--fraction", default="1.0", help="公開率 (>=1.0 で完了)")
    sp.add_argument("--halt", action="store_true", help="段階公開を停止 (halted)")
    sp.add_argument("--dry-run", action="store_true")
    sp.set_defaults(func=cmd_rollout)

    sp = sub.add_parser("notes-new", help="全言語のリリースノート雛形を一括生成")
    sp.add_argument("--version", type=int, default=None, help="versionCode (既定: app-config.json)")
    sp.set_defaults(func=cmd_notes_new)

    sp = sub.add_parser("notes-check", help="全言語そろい + 500 文字以内かを検査")
    sp.add_argument("--version", type=int, default=None)
    sp.set_defaults(func=cmd_notes_check)

    args = p.parse_args()
    try:
        args.func(args)
    except Exception as e:
        try:
            from googleapiclient.errors import HttpError
        except ImportError:
            HttpError = ()
        if HttpError and isinstance(e, HttpError):
            import json as _json
            code = getattr(e, "status_code", None) or (
                e.resp.status if getattr(e, "resp", None) else "?")
            msg, url = str(e), None
            try:
                data = _json.loads(e.content.decode("utf-8"))
                err = data.get("error", {})
                msg = err.get("message", msg)
                for d in err.get("details", []):
                    for link in d.get("links", []):
                        if link.get("url"):
                            url = link["url"]
            except Exception:
                pass
            hint = "\n  有効化: {}".format(url) if url else ""
            die("Play API エラー (HTTP {}):\n  {}{}".format(code, msg, hint))
        raise


if __name__ == "__main__":
    main()
