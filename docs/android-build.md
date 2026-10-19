# ビルドと実行 (Makefile / Gradle)

リポジトリ直下の [Makefile](../Makefile) から `gradlew` を呼んで、ビルド・インストール・起動・デバッグ・
テストなどを行えます。Gradle を直接呼ぶこともできます。

## 前提

- GNU Make が使えるシェル (Git Bash / MSYS2 / WSL など。Windows では `mingw32-make` でも可)
- Android SDK の `adb` は `local.properties` の `sdk.dir` から自動で探します。見つからないときは `ADB` で指定します

## Makefile のターゲット

| ターゲット | 内容 |
|---|---|
| `build` / `assemble` | `assemble<BUILD_TYPE>` (既定は Debug) |
| `bundle` | `bundle<BUILD_TYPE>` (AAB) |
| `release-debug` / `install-release-debug` | リリース構成を debug の鍵で署名した APK を作る / 入れる (リリース用の鍵は不要。Play には出せない) |
| `install` / `reinstall` / `uninstall` | 端末へのインストール / 入れ直し / アンインストール |
| `run` | ランチャーの Activity を起動 |
| `debug` | デバッガ待ちで起動 (`am start -D`)。あとから IDE でアタッチ |
| `stop` | アプリを強制停止 |
| `logcat` | `adb logcat` を流し続ける (Ctrl+C で終わる) |
| `clean` | gradle clean (`build/` の下を消す) |
| `clean-cmake` | CMake の生成物 (`.cxx/`、`build/intermediates/cxx`、`build/generated/cmake`) だけを消す。プラグイン構成を変えたあとやプロジェクトを切り替えたあとに |
| `distclean` | `clean` + `clean-cmake`。チェックアウト直後と同じ状態に戻す (vcpkg の install と CMake の configure をやり直す) |
| `test` / `connected` | ユニットテスト / 計測テスト |
| `devices` | 接続している端末の一覧 |
| `vars` | 変数の解決結果を表示 |
| `pad-install` / `pad-fast` / `pad-aab` / `pad-package` | Play Asset Delivery の検証・配布 ([PAD](android-play-asset-delivery.md)) |
| `repl` / `repl-py` | adb 経由の TJS 対話シェル (Debug ビルドのみ。[REPL](android-repl.md)) |
| `play-status` / `play-upload` / `play-promote` / … | Play Console の操作 ([公開自動化](android-play-publishing.md)) |

上書きできる変数:

| 変数 | 内容 |
|---|---|
| `PROJECT_DIR` | プロジェクトフォルダ ([設定](android-config.md)) |
| `MODULE` | 対象のモジュール名 (既定: `app`) |
| `BUILD_TYPE` | `Debug` / `Release` (既定: `Debug`) |
| `APP_ID` | アプリ ID。`app/build.gradle` の `applicationId` → `AndroidManifest.xml` の `package` の順に自動で探す |
| `ADB` | `adb` のパス。未指定なら `sdk.dir` から推測し、無ければ `adb` |
| `GRADLEW` | `gradlew` の呼び方 (既定: `./gradlew`) |

```bash
make build                              # ビルド (Debug)
make build BUILD_TYPE=Release           # Release
make run                                # インストールして起動
make debug                              # デバッガ待ちで起動して IDE からアタッチ
make logcat                             # ログを表示
make build PROJECT_DIR=/path/to/mygame  # 別のプロジェクトフォルダ
make run APP_ID=com.example.mygame      # アプリ ID を明示
make devices ADB="/c/Android/sdk/platform-tools/adb"   # adb を明示
```

## Gradle を直接呼ぶ

```bash
./gradlew assembleDebug     # デバッグビルド (APK)
./gradlew installDebug      # 接続した端末へ入れる
./gradlew assembleRelease   # リリースビルド (署名の設定が要る。署名を参照)
./gradlew bundleRelease     # Google Play 用の AAB
```

APK は `${PROJECT_DIR}/build/android/app/outputs/apk/<variant>/` に出ます
(プロジェクトフォルダを指定しないときはリポジトリ直下の `build/android/...`)。
リリースビルドの署名は [リリース用の署名](android-signing.md) を参照してください。
