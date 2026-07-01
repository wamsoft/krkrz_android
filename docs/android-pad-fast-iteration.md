# PAD 軽量イテレーション (`make pad-fast`) — 仕組みと処理構造

`make pad-fast` の設計メモ。**PAD 構成の案件でコード (Java/native/TJS) だけ直すときに、
数 GB のパックを毎回端末へ再 push せず、変わったものだけ反映する**ための経路。
使い方の要約は [android-play-asset-delivery.md](android-play-asset-delivery.md#軽量イテレーション-make-pad-fast--コードだけ直すとき) を参照。
ここでは「なぜ動くのか」「何をどう処理しているか」を残す。

---

## 1. 背景 — なぜ `make pad-install` だと重いか

`make pad-install` (= `bundletool build-apks --local-testing` → `install-apks`) は、
**毎回すべての asset pack (案件によっては数 GB) を端末へ push** する。コードを 1 行直すたびに
これをやると数分かかる。一方、素の `make run` (`assembleDebug` + `adb install`) は速いが、
PAD 構成では **APK にパックデータが入らず、local-testing の配線も無い**ので、
起動時に PAD 解決が失敗してゲームデータが見つからない (→ [android-play-asset-delivery.md] 参照)。

`make pad-fast` は両者の良いとこ取り: **素の APK でも既に端末にあるパックを読める**ようにし、
**変わったパックだけ**を差し替える。

---

## 2. local-testing の実体 (実機実測)

`make pad-fast` の前提になっている bundletool `--local-testing` の挙動を、
実機で実測して確定させたもの (パッケージ名は例として `<pkg>` と表記)。

### 2.1 base APK 側の配線 (meta-data)

`bundletool build-apks --local-testing` は base manifest に次を注入する:

```xml
<meta-data android:name="local_testing_dir" android:value="local_testing" />
<meta-data android:name="com.android.vending.splits.required" android:value="true" />
```

- **`local_testing_dir` が肝**。これがあると Play asset-delivery ライブラリは
  「Play からの配信」ではなく **`getExternalFilesDir()/local_testing/` からパックを読む**
  ローカルテストモードになる。
- **実測: `local_testing_dir` だけあれば `splits.required` 無しでも local testing は動く**。
  → だから `make pad-fast` は `local_testing_dir` の 1 個だけを素の APK に足せばよい。

### 2.2 端末上のファイル配置

| 役割 | パス | 所有 / 触り方 |
|---|---|---|
| パック本体 (push 先) | `/sdcard/Android/data/<pkg>/files/local_testing/<pack>-master.apk` | **shell 所有・`adb push`/`rm` 可** (run-as 不要) |
| パック展開先 | `/data/data/<pkg>/files/assetpacks/<pack>/<appVer>/<packVer>/assets/…` | アプリ所有 (`run-as`) |
| engine が読むルート | `/data/data/<pkg>/files/padroot/` (symlink farm) | アプリ所有 (`run-as`) |

- asset pack は **`<pack>-master.apk` の 1 本のみ** (assets には abi/density split が無い)。
  なので push 対象は `<pack>-master.apk` だけでよい。
- ライブラリは起動時の `fetch()` で `local_testing/<pack>-master.apk` を読み、
  `files/assetpacks/<pack>/<appVer>/<packVer>/assets/` へ **展開**する。
- **展開のキーは `<appVer>/<packVer>` = `1/1` 固定**
  (versionCode を上げていないため)。→ **重要な落とし穴**は §4.3。

### 2.3 padroot symlink farm

`PadAssetResolver.buildSymlinkFarm` が **毎起動** `files/padroot/` を作り直し、
各パックの `assetsPath()` 配下を相対パスを保ったまま symlink で束ねる (コピー無し・先勝ち)。
engine には `padroot` の絶対パスを `setAssetCacheDir` で渡す。
展開先を差し替えても padroot は毎起動作り直されるので追従する。

```
files/padroot/data.xp3   -> …/assetpacks/coredata/1/1/assets/data.xp3
files/padroot/fgimage.xp3-> …/assetpacks/imagedata/1/1/assets/fgimage.xp3
files/padroot/data/video -> (videodata パックの mirror)
```

---

## 3. 処理構造 (`scripts/pad-fast-install.sh`)

```
make pad-fast
  └─ scripts/pad-fast-install.sh   (共通処理は scripts/pad-common.sh)
       [1] ./gradlew :app:assembleDebug -PpadLocalTesting
       │      → base APK に local_testing_dir meta-data を注入 (§4.1)
       │      → packPad_<name> も走り assetpacks/<name>/src/main/assets を最新化
       [2] adb install -r -d <app-debug.apk>        # 1 APK だけ・数秒
       [3] pack 差分チェック (各パック):
       │      h    = assets ツリーの内容ハッシュ (§4.2)
       │      prev = assetpacks/.fastpush/<name>.sha
       │      ondev= 端末に local_testing/<name>-master.apk があるか
       │      h==prev かつ ondev → skip / それ以外 → CHANGED に積む
       [4] CHANGED があれば:
       │      ./gradlew :app:bundleDebug            # AAB を作り直す
       │      bundletool build-apks --local-testing → pad-fast.apks
       │      各 CHANGED パックについて:
       │        - .apks から <name>-master.apk を取り出す (python zipfile)
       │        - adb push → local_testing/<name>-master.apk
       │        - run-as rm -rf files/assetpacks/<name>   # 再展開強制 (§4.3)
       │        - assetpacks/.fastpush/<name>.sha を更新
       │      CHANGED が空なら pack には一切触らない (端末の既存展開を再利用)
       [5] prune: 現行 padPacks に無い files/assetpacks/<x> を削除 (rename/削除の後始末)
       [6] adb shell monkey … LAUNCHER              # 起動
```

初回の種まき: **`make pad-install` が install 後に各パックのマーカーを書く**ので、
一度 `pad-install` すれば以後 `pad-fast` は「変更なし」を検出できる。
マーカーが無い / パック名を変えた初回は全 push になる (以後は速い)。

---

## 4. 設計判断とキモ

### 4.1 meta-data の注入ゲート (release に絶対漏らさない)

`local_testing_dir` が **release AAB に入ると本番の PAD 配信が壊れる** (ライブラリが
存在しない local_testing/ を見に行く)。そこで:

- overlay を **`app/src/padlocal/AndroidManifest.xml`** に置き、
- `app/build.gradle` で **`-PpadLocalTesting` が指定されたときだけ**
  `android.sourceSets.getByName('debug').manifest.srcFile(...)` で debug ソースセットに載せる。

これで:
- `make pad-fast` (= `assembleDebug -PpadLocalTesting`) にだけ meta-data が入る。
- `make build` / `make run` / release / bundle には**一切入らない**。
- `make pad-install` (bundleDebug 経由) はこのプロパティを渡さないので、bundletool が
  自前で注入する local_testing_dir と**二重にならない**。

> 注意: overlay manifest の**コメントに `--` を含めない**こと (XML コメントは `--` 禁止で
> manifest merger が `SAXParseException` になる。実際に `--local-testing` と書いて踏んだ)。

### 4.2 パック差分の判定 (内容ハッシュ)

`pad_pack_hash` = `find . -type f -printf '%P|%s|%T@' | sort | sha256sum`
(パス・サイズ・mtime)。ハッシュが安定する理由:

- `packPad_<name>` のコピー本体 `runCopyRules` は **`COPY_ATTRIBUTES` で mtime を維持**し、
  **変更ファイルだけ**再コピーする。→ データ未変更なら dest の mtime も不変 → ハッシュ不変。

マーカーは `<PROJECT_DIR>/build/android/assetpacks/.fastpush/<name>.sha`。
「最後に端末へ push した内容」を表す。端末側の `local_testing/<name>-master.apk` の
存在チェックと **AND** で判定する (マーカーがあっても端末を初期化していたら push する)。

### 4.3 同 version では再展開されない → 展開先を消す

展開キーが `1/1` 固定 (§2.2) なので、**同じ versionCode のまま `<pack>-master.apk` を
差し替えても、ライブラリは `files/assetpacks/<pack>/1/1/` が既にあると再展開しない** =
古いデータのまま。これを避けるため、パックを push したら
**`run-as <pkg> rm -rf files/assetpacks/<name>`** で展開先を消し、次回起動で再展開させる。

### 4.4 署名の一致

`assembleDebug` の署名 (AGP の `~/.android/debug.keystore`) と、端末に入っている
アプリの署名が一致している必要がある。`make pad-install` も同じ debug keystore を使うので
通常は一致 → `adb install -r` で上書きできる。不一致なら `adb install -r` が失敗するので、
一度 `make pad-install` (または `make reinstall`) でクリーン導入する。

---

## 5. ついでに直した既存バグ (symlink farm)

`PadAssetResolver.buildSymlinkFarm` は padroot を作り直す前に `deleteRecursively` で
丸ごと消すが、旧実装は **ぶら下がり symlink (リンク先が既に消えたもの) を削除できなかった**:

- `File#exists()` は **symlink を辿る**ので、リンク先が消えた symlink を「存在しない」と
  誤判定し、削除をスキップ → 残った古い symlink に対して `Os.symlink` が
  **`EEXIST (File exists)`** で落ちる。
- 発生条件: パックの展開先を消した後 (§4.3) や、パック名を rename した後の起動
  (旧 padroot の symlink がぶら下がる)。→ `make pad-fast` で必ず踏む。

修正: 削除判定を **`Files.exists(path, NOFOLLOW_LINKS)` / `Files.isSymbolicLink`** ベースにし、
実ディレクトリだけ再帰・symlink はリンク自体を消す。`linkTree` の先勝ち判定も NOFOLLOW に。

---

## 6. 制約・注意

- **Debug 専用**。`local_testing_dir` は debug manifest overlay。Release の実機検証は
  `make pad-install VARIANT=Release` (bundletool 経由・要 Release 署名)。
- **先に一度 `make pad-install`** が必要 (パック配置 + マーカー種まき)。
- パック変更時は結局 `bundleDebug` + `build-apks` で全パックを一度ローカル生成する
  (再 zip 分の時間はかかる)。**削減できるのは端末への push (数 GB の adb 転送) の部分**。
- `python` (zip 抽出) と `java` (bundletool, パック変更時のみ) が要る。
- 現行 `padPacks` に無い古いパックは端末から自動 prune する (§3 [5])。
- 端末側のファイル確認・手動削除は [android-adb-tips.md](android-adb-tips.md)。

---

## 7. 関連ファイル

| ファイル | 役割 |
|---|---|
| `scripts/pad-fast-install.sh` | 本体 (§3 のフロー) |
| `scripts/pad-common.sh` | 共通ヘルパ (adb 解決 / パック列挙 / ハッシュ / パス) |
| `scripts/pad-local-install.sh` | フル導入。install 後にマーカーを書く |
| `app/src/padlocal/AndroidManifest.xml` | `local_testing_dir` overlay |
| `app/build.gradle` | `-PpadLocalTesting` ゲート |
| `app/src/main/java/jp/wamsoft/krkrz/PadAssetResolver.java` | fetch / 展開解決 / symlink farm |
| `Makefile` | `pad-fast` ターゲット |
