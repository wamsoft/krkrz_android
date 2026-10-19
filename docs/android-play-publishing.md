# Google Play Console 公開自動化

コンソールを開かず、CLI / `make` から Play Console へ **AAB アップロード / 現状照会 / トラック昇格 /
段階公開 / リリースノート管理** を行う仕組み。Play Developer API v3 (androidpublisher) を Python
スクリプト (`scripts/play-publish.py`) で直接叩く。

- ビルド非依存: `make pad-aab` / `bundleRelease` で作った既存の release AAB をそのまま上げる。
- 読み書き両対応: `play-status` は読み取り専用でコンソール不要の現状確認に使える。
- リリースノートは **サポート全言語を一括管理** (書き漏れ防止)。

---

## 1. 前提セットアップ (最初に一度)

### 1-1. Python 依存

```bash
python -m pip install -r scripts/requirements-play.txt
# google-api-python-client / google-auth (+ OAuth 方式を使うなら google-auth-oauthlib)
```

### 1-2. 認証 (2 方式・自動判定)

認証は用途で使い分ける。**どちらか一方 (または両方) を `local.properties` に設定**すればよい。

| | OAuth ユーザー認可 | サービスアカウント |
|---|---|---|
| 向き | **ローカル手動リリース** | **CI / 無人** |
| 認証主体 | 自分の Google アカウント | 専用の非人間アカウント |
| ブラウザ | 初回だけ同意 → 以後 refresh token 再利用 | 不要 (JSON 鍵のみ) |
| Play 権限 | 認可するアカウントが既に持つ権限を使う | サービスアカウントを招待して権限付与が必要 (要管理者) |

判定順: `PLAY_AUTH=service|oauth` で明示 → 無ければ `PLAY_SERVICE_ACCOUNT_JSON` があればサービス
アカウント、無く `PLAY_OAUTH_CLIENT_JSON` があれば OAuth。**両方あれば CI 向けにサービスアカウント優先**。

#### (A) OAuth ユーザー認可 (ローカル向け)

自分の Google アカウントが既に Play Console の権限を持っていれば、サービスアカウントの招待を
待たずに使える。

1. [Google Cloud Console](https://console.cloud.google.com/) →「API とサービス」→「認証情報」→
   **OAuth クライアント ID を作成 (種類: デスクトップ アプリ)** → JSON をダウンロード。
2. 「OAuth 同意画面」を設定し、自分のアカウントを**テストユーザー**に追加
   (「テスト」ステータスだと refresh token は 7 日で失効。継続運用は「本番」に公開)。
3. `local.properties` にクライアント JSON のパスを記載:

```properties
# ローカル手動リリース (OAuth)
PLAY_OAUTH_CLIENT_JSON=C:/Users/you/keys/play-oauth-client.json
```

初回の `make play-status` でブラウザが開き同意 → refresh token が
`${BUILD_SYSTEM_DIR}/.play-oauth-token.json` (`.gitignore` 済) にキャッシュされ、以後は無人で更新
される (`PLAY_OAUTH_TOKEN=` で保存先変更可)。

#### (B) サービスアカウント (CI 向け)

1. [Google Cloud Console](https://console.cloud.google.com/) で**サービスアカウント**を作成 →
   **JSON 鍵**を発行してダウンロード。
2. [Play Console](https://play.google.com/console/) →「ユーザーと権限」→ そのサービスアカウントの
   メールアドレスを招待し、対象アプリへのリリース権限を付与 (**要管理者権限**)。
3. `local.properties` (または CI では環境変数) に鍵パスを記載:

```properties
# CI / 無人 (サービスアカウント)
PLAY_SERVICE_ACCOUNT_JSON=C:/Users/you/keys/play-service-account.json
```

`${PROJECT_DIR}` 等の展開も効く。CI では JSON をファイルに書き出し、そのパスを環境変数
`PLAY_SERVICE_ACCOUNT_JSON` で渡す。

> 初回アップロードだけは Play Console で手動が必要な場合がある (アプリの新規作成・内部テスト
> 有効化など)。以降の更新は本ツールで回せる。

### 1-3. 動作確認

```bash
make play-status     # 各トラックの現状が表示されれば認証OK (何も無ければ空表示)
```

---

## 2. できること (概要)

| やりたいこと | コマンド |
|---|---|
| 現状照会 (versionCode / 状態 / 公開率 / ノート) | `make play-status` |
| AAB を internal テストへ上げる | `make play-upload` |
| AAB を本番へ 10% 段階公開で上げる | `make play-upload TRACK=production ROLLOUT=0.1` |
| internal で確認済のものを本番へ昇格 (再アップロード無し) | `make play-promote FROM=internal TO=production` |
| 本番へ昇格 + 20% 段階公開 | `make play-promote FROM=internal TO=production ROLLOUT=0.2` |
| 段階公開率を 50% に上げる | `make play-rollout FRACTION=0.5` |
| 段階公開を全公開にする | `make play-rollout FRACTION=1.0` |
| 段階公開を停止 (問題発覚時) | `make play-rollout HALT=1` |
| 全言語のリリースノート雛形を作る | `make play-notes-new` |
| リリースノートの過不足を検査 | `make play-notes-check` |

### トラックについて

`internal` (内部テスト) → `alpha` (クローズドテスト) → `beta` (オープンテスト) → `production` (本番)。
本ツールの既定トラックは **`internal`** (`TRACK=` で上書き)。`play-rollout` だけは段階公開が
本番系の概念なので既定 `production`。

### テスト → 本番の典型フロー

```bash
# 1. AAB を作る (PAD 構成)
make pad-aab

# 2. リリースノートを用意して internal へ
make play-notes-new                 # 雛形生成 → 各言語ファイルに記入
make play-notes-check               # 全言語そろい & 500字以内を確認
make play-upload                    # internal テストへ (既定 status=completed)

# 3. 実機で内部テスト確認後、本番へ 10% 段階公開で昇格 (再アップロード不要)
make play-promote FROM=internal TO=production ROLLOUT=0.1

# 4. 問題なければ公開率を上げて全公開
make play-rollout FRACTION=0.5
make play-rollout FRACTION=1.0
# 問題があれば停止
make play-rollout HALT=1
```

### 差分アップロードについて

開発者は **毎回フルの AAB をアップロード** する (差分アップロード API は無い)。ユーザー端末への
配信は Google 側が自動で差分 (delta) 化するのでユーザーの DL は小さい。PAD の asset pack も
未変更パックは端末に再配信されない。`versionCode` は毎回**単調増加**が必須
(`app-config.json` の `versionCode` を上げる)。

---

## 3. リリースノート運用

versionCode 別・言語別のテキストファイルを **プロジェクトフォルダで管理**する:

```
${PROJECT_DIR}/dist/playstore/whatsnew/<versionCode>/
    en-US.txt      # 英語 (既定言語)
    ja-JP.txt      # 日本語
    zh-CN.txt      # 簡体字
    zh-TW.txt      # 繁体字
    default.txt    # 未記入言語のフォールバック (任意)
```

- **言語リストはアプリの res から自動導出** (`app/src/main/res` とタイトルの `resOverride` の
  `values-*` を Play 言語コードへマップ)。`values`→`en-US` / `values-ja`→`ja-JP` /
  `values-zh-rCN`→`zh-CN` / `values-zh-rTW`→`zh-TW`。専用の設定キーは持たない (単一ソース)。
- `make play-notes-new [VERSION=N]` で **全言語ぶんの空ファイルを一括生成** (VERSION 省略時は
  `app-config.json` の versionCode)。各ファイルに **500 文字以内** で記入する。
- `make play-notes-check [VERSION=N]` で全言語そろい + 文字数を検査 (CI に組み込める)。
- `play-upload` / `play-promote` は対象 versionCode のノートを自動で添付する。ある言語の
  ファイルが空/無い場合は `default.txt` があればそれを使う。

### 下書きに git / svn ログを使う

リリースノートの**下書き生成**にはログが使えるが、そのまま登録はしない (開発者向けの文言が
混ざるため)。ユーザー向けに書き直し + 翻訳してから各言語ファイルへ。

```bash
# 前タグからの変更を下書きに (このリポジトリ = ビルドシステムの変更のみ)
git log --oneline <前タグ>..HEAD > ${PROJECT_DIR}/dist/playstore/whatsnew/<vc>/en-US.txt
# ゲーム本体 (marco 等) は SVN 管理なので svn log から
svn log -r <前rev>:HEAD ${PROJECT_DIR}
```

> internal トラックは下書きのままでも可。production は必ずユーザー向けに整えて全言語翻訳する。

---

## 4. 安全確認 (dry-run)

`upload` / `promote` / `rollout` は `DRYRUN=1` で **commit せず検証 (validate) だけ**行える。
API 呼び出しの内容とトラック割り当てが正しいか、実際に反映する前に確認できる。

```bash
make play-upload DRYRUN=1
make play-promote FROM=internal TO=production ROLLOUT=0.1 DRYRUN=1
```

---

## 5. スクリプトを直接使う (make が無い環境 / CI)

```bash
PROJECT_DIR=/path/to/project python scripts/play-publish.py status
PROJECT_DIR=/path/to/project python scripts/play-publish.py upload --track internal
python scripts/play-publish.py promote --from internal --to production --rollout 0.1
python scripts/play-publish.py rollout --track production --fraction 1.0
python scripts/play-publish.py notes-new --version 5
```

各コマンドの引数は `python scripts/play-publish.py <cmd> --help`。

---

## 6. API フロー (内部)

`upload` は Play Developer API v3 の edit フローで動く:

1. `edits.insert` — 編集セッション開始
2. `edits.bundles.upload` — AAB を resumable アップロード (数 GB 対応)、`versionCode` が返る
3. `edits.tracks.update` — トラックに release (versionCode + status + releaseNotes) を割り当て
4. `edits.validate` → `edits.commit` — 検証して確定 (dry-run は commit せず破棄)

`promote` は 2 を飛ばし、昇格元トラックの versionCode を昇格先の release に足して commit
(再アップロード不要)。`status` は insert → tracks.list → 破棄 (commit しない) の読み取り専用。

---

## 7. トラブルシュート

| 症状 | 対処 |
|---|---|
| `認証情報が未設定` | `local.properties` に `PLAY_OAUTH_CLIENT_JSON=` (ローカル) か `PLAY_SERVICE_ACCOUNT_JSON=` (CI) を設定 (§1-2) |
| OAuth で毎回ブラウザ同意を求められる / 数日で切れる | 同意画面が「テスト」だと refresh token が 7 日で失効。継続運用は同意画面を「本番」に公開 (§1-2 A) |
| `依存パッケージが未インストール` | `python -m pip install -r scripts/requirements-play.txt` |
| `403` / permission 系 | Play Console「ユーザーと権限」でサービスアカウントに権限付与 (§1-2) |
| `The version code N has already been used` | `app-config.json` の `versionCode` を上げて AAB を作り直す |
| `APK/AAB signed with wrong key` | アップロード鍵で署名した AAB を使う (`local.properties` の RELEASE_* 参照) |
| `releaseNotes` が反映されない | `dist/playstore/whatsnew/<vc>/<locale>.txt` の言語コードが Play の対応言語か確認 |
| 段階公開の対象が無い (`rollout`) | 先に `play-upload`/`promote` で `ROLLOUT=` 付き (inProgress) にしておく |

関連: AAB の作り方は [docs/android-play-asset-delivery.md](android-play-asset-delivery.md)、
署名は [README](../README.md) の「Release (Google Play 用署名)」。
