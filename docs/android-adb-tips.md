# adb 操作 TIPS（ファイル確認・削除・アプリ状態）

adb でのファイル確認や削除の手順を毎回忘れがちなのでまとめた実務用メモ。
このプロジェクト（KrkrZ Android / PAD 構成）でよく触る場所を中心に、コピペで使える形にしてある。

- Windows では adb は `adb.exe`。PATH に無ければ SDK の実体を使う:
  `C:\Users\<you>\AppData\Local\Android\Sdk\platform-tools\adb.exe`
- 以降 `PKG` はアプリの applicationId（例: `jp.example.app`）。`app-config.json` の `applicationId`。
- `$(…)` などのシェル記法は Git Bash 前提。PowerShell の場合は後半の「PowerShell から使う」を参照。

---

## 0. まず端末とアプリの基本確認

```bash
adb devices -l                                   # 接続中の端末一覧
adb shell getprop ro.product.model               # 端末モデル
adb shell pm list packages | grep example        # インストール済みか（部分一致）
adb shell dumpsys package PKG | grep versionCode # 入っているバージョン
adb shell pm path PKG                            # インストール済み APK/split の実パス一覧
```

---

## 1. どこに何が置かれるか（このプロジェクトの地図）

| 用途 | パス | 見る手段 |
|---|---|---|
| アプリ内部データ | `/data/data/PKG/files/` | `run-as PKG`（**debuggable のみ**） |
| 　assets コピーキャッシュ | `/data/data/PKG/files/assets/` | 〃 |
| 　PAD 展開 pack | `/data/data/PKG/files/assetpacks/<pack>/<appVer>/<packVer>/assets/` | 〃 |
| 　PAD symlink farm（engine が読む） | `/data/data/PKG/files/padroot/` | 〃 |
| 　セーブ等 | `/data/data/PKG/files/*.ksd` ほか | 〃 |
| PAD local_testing の pack 本体 | `/sdcard/Android/data/PKG/files/local_testing/<pack>-master.apk` | 通常の `ls`（run-as 不要） |
| インストール済み APK/split | `pm path PKG` で出るパス（`/data/app/...`） | 〃 |

**ポイント**: `/data/data/PKG/...`（内部ストレージ）は他アプリから見えないので **`run-as PKG` 経由**でしか触れない。
`run-as` は **debuggable ビルド**（Debug APK）でのみ使える。Release では不可。
`/sdcard/Android/data/PKG/...`（外部ストレージのアプリ専用領域）は `run-as` 無しで `ls`/`push`/`rm` できる。

---

## 2. ファイル確認（見る）

```bash
# 外部ストレージ（run-as 不要）
adb shell ls -la /sdcard/Android/data/PKG/files/local_testing/

# 内部ストレージ（run-as 必須）
adb shell run-as PKG ls -la files/
adb shell run-as PKG ls -la files/assetpacks/

# 再帰的にツリーを見る（深さ制限つき。件数が多いと重いので maxdepth 推奨）
adb shell run-as PKG find files/padroot -maxdepth 2

# symlink の実体を確認（PAD の padroot は symlink farm）
adb shell run-as PKG ls -la files/padroot/
#   例: data.xp3 -> /data/data/PKG/files/assetpacks/coredata/1/1/assets/data.xp3

# ファイルの中身を覗く（テキスト/JSON など小物）
adb shell run-as PKG cat files/some.json

# ディレクトリの合計サイズ（何が容量を食っているか）
adb shell run-as PKG du -sh files/*
adb shell du -sh /sdcard/Android/data/PKG/files/local_testing/*
```

`run-as PKG` の後は **カレントがアプリのホーム `/data/data/PKG/`** になるので、
`files/...` のように相対で書ける（`/data/data/PKG/files/...` とフルで書いてもよい）。

---

## 3. ファイル削除（消す）

```bash
# 1 ファイル削除
adb shell run-as PKG rm files/assets/foo.xp3
adb shell rm /sdcard/Android/data/PKG/files/local_testing/coredata-master.apk

# ディレクトリごと削除（-rf）
adb shell run-as PKG rm -rf files/assetpacks/coredata     # この pack を再展開させたいとき
adb shell run-as PKG rm -rf files/assets                  # assets コピーキャッシュを作り直させる

# アプリのデータを丸ごと消す（アンインストールせずに初期化）
adb shell pm clear PKG
#   → files/ databases/ shared_prefs/ すべて消える。次回起動は「新規インストール直後」状態。
#     ただし /sdcard/Android/data/PKG/ は残ることがある（下記で別途消す）
adb shell rm -rf /sdcard/Android/data/PKG/files/local_testing   # PAD の pack 本体も消す場合
```

**このプロジェクトでよく使う削除**:

- **PAD の pack を作り直したい / 再展開させたい** → 該当 pack の展開先を消す:
  `adb shell run-as PKG rm -rf files/assetpacks/<pack>`
  次回起動でライブラリが `local_testing/<pack>-master.apk` から再展開する。
  （`make pad-fast` は pack を再 push したときに自動でこれをやる）
- **assets コピーキャッシュを作り直したい**（Bootstrap が copy する経路）:
  `adb shell run-as PKG rm -rf files/assets` → 次回起動で全コピーし直し。
- **古い pack 名のゴミ掃除**（`padPacks` を rename した後など）:
  現行 config に無い `files/assetpacks/<oldname>` と
  `/sdcard/Android/data/PKG/files/local_testing/<oldname>-master.apk` を消す。

> 注意: `pm clear` はセーブデータも消える。特定ファイルだけ消したいときは `rm` をピンポイントで。

---

## 4. ファイルの出し入れ（push / pull）

```bash
# 端末 → PC に吸い出す（例: セーブデータやログを回収）
adb pull /sdcard/Android/data/PKG/files/local_testing/coredata-master.apk ./coredata-master.apk
adb shell run-as PKG cat files/save1.ksd > save1.ksd     # run-as 領域は pull できないので cat で吸う

# PC → 端末に送る（外部ストレージは push 可、run-as 領域は直接 push 不可）
adb push ./coredata-master.apk /sdcard/Android/data/PKG/files/local_testing/coredata-master.apk

# run-as 領域へ入れたいときは一旦 /data/local/tmp を経由して run-as cp
adb push ./foo.json /data/local/tmp/foo.json
adb shell run-as PKG cp /data/local/tmp/foo.json files/foo.json
```

`run-as PKG` 領域には `adb push` で**直接は書けない**（権限が別）。上のように tmp 経由で `cp` する。

---

## 5. アプリのインストール / 起動 / 停止

```bash
adb install -r app-debug.apk        # 上書きインストール（データ保持）
adb install -r -d app-debug.apk     # -d: versionCode 据え置き/ダウングレードも許可
adb uninstall PKG                   # アンインストール（データも消える）

adb shell monkey -p PKG -c android.intent.category.LAUNCHER 1   # ランチャー起動（Activity 名不要）
adb shell am force-stop PKG                                     # 強制終了
adb shell am start -D -a android.intent.action.MAIN -c android.intent.category.LAUNCHER -p PKG  # デバッガ待ちで起動
```

`-r` 上書きは**署名が一致している**ことが前提。署名が違うと `INSTALL_FAILED_UPDATE_INCOMPATIBLE` →
一度 `adb uninstall PKG` してから入れ直す（＝データは消える）。

---

## 6. ログ（logcat）

```bash
adb logcat                                         # 全部（多い）
adb logcat -s BootstrapActivity:* PadAssetResolver:*   # タグで絞る（PAD 周りはこれ）
adb logcat *:E                                     # エラーだけ
adb logcat -c                                      # バッファをクリアしてから見ると読みやすい
adb logcat --pid=$(adb shell pidof PKG)            # 対象アプリのプロセスだけ
```

---

## 7. PowerShell から使う（Windows）

Git Bash が無い場合。`run-as` の複数コマンドは `sh -c '...'` でまとめると楽。

```powershell
$adb = "C:\Users\go\AppData\Local\Android\Sdk\platform-tools\adb.exe"
$pkg = "jp.example.app"

& $adb shell "run-as $pkg ls -la files/assetpacks/"
& $adb shell "run-as $pkg rm -rf files/assetpacks/coredata"
& $adb shell "ls -la /sdcard/Android/data/$pkg/files/local_testing/"
& $adb shell "run-as $pkg du -sh files/*"
```

- PowerShell では `adb` に渡すシェル片は**ダブルクオートで丸ごと 1 引数**にすると `/` やスペースで事故りにくい。
- ワイルドカードや `$(…)` は**端末側シェルに評価させたい**ので、`"..."` の中に入れて渡す（PowerShell 側で展開させない）。

---

## 8. よくハマる点

- **`run-as: Package 'PKG' is not debuggable`** → Release ビルドを見ている。Debug を入れるか、内部ストレージは諦めて logcat / 外部ストレージで確認する。
- **`/sdcard/...` は消せるのに `/data/data/...` が Permission denied** → `run-as PKG` を付け忘れ。
- **`adb push` が `/data/data/...` に入らない** → 直接は不可。`/data/local/tmp` 経由で `run-as cp`（§4）。
- **`ls` で何も出ない / ディレクトリ判定が変** → Android の `ls` は簡易版。`find <dir> -maxdepth N` の方が確実。
- **削除したのに反映されない** → アプリが起動中でファイルを掴んでいることがある。`am force-stop PKG` してから消す。
- **`pm clear` してもデータが残る感じがする** → `/sdcard/Android/data/PKG/` は別途 `rm -rf` が要る場合がある。

---

参考: PAD の仕組みと `make pad-install` / `make pad-fast` は
[docs/android-play-asset-delivery.md](android-play-asset-delivery.md) を参照。
