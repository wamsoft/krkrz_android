# リリース用の署名

Google Play にアプリを出すには、リリース用の鍵で署名した APK / AAB が要ります。
ここでは鍵の作り方・設定の仕方・確認の仕方と、鍵を用意せずにリリース構成を試す方法を説明します。
Play Console へのアップロードの自動化は [Google Play Console 公開自動化](android-play-publishing.md) を参照してください。

## 1. キーストアを作る

`keytool` でキーストア (`.jks`) を作ります。

```bash
keytool -genkey -v -keystore release-key.jks -keyalg RSA -keysize 2048 -validity 10000 -alias my-key-alias
```

| オプション | 説明 |
|---|---|
| `-keystore` | 作るキーストアのファイル名 |
| `-keyalg` | 鍵のアルゴリズム (RSA を推奨) |
| `-keysize` | 鍵の長さ (2048 以上を推奨) |
| `-validity` | 有効日数 (10000 日 ≒ 27 年) |
| `-alias` | 鍵の別名 |

実行するとキーストアのパスワード・名前や組織名など・鍵のパスワード (キーストアと同じでも可) を聞かれます。

> **重要**: キーストアとパスワードは失くさないよう安全に保管してください。Google Play に上げたアプリを
> 更新するには同じ鍵が要ります。

## 2. 署名情報を設定する

`local.properties` に書きます。このファイルは `.gitignore` の対象なので、パスワードがコミットされることは
ありません。`app/build.gradle` は `local.properties` を読んで署名に使うようになっているので、追記するだけです。

```properties
# Release signing
RELEASE_STORE_FILE=../release-key.jks
RELEASE_STORE_PASSWORD=your_keystore_password
RELEASE_KEY_ALIAS=my-key-alias
RELEASE_KEY_PASSWORD=your_key_password
```

CI など `local.properties` が無い環境では、同じ名前の環境変数でも指定できます。

キーストア自体もコミットしないよう、`.gitignore` に足しておきます。

```gitignore
# Signing keys
*.jks
*.keystore
```

## 3. 署名したリリースビルドを作る

```bash
./gradlew assembleRelease   # 署名済み APK
./gradlew bundleRelease     # Google Play 用の App Bundle (推奨)
```

- APK: `${PROJECT_DIR}/build/android/app/outputs/apk/release/app-release.apk`
- AAB: `${PROJECT_DIR}/build/android/app/outputs/bundle/release/app-release.aab`

(ファイル名は `app-config.json` の `apk.baseName` で変えられます。[設定](android-config.md) を参照)

## 4. 署名を確かめる

```bash
apksigner verify --verbose "${PROJECT_DIR:-.}/build/android/app/outputs/apk/release/app-release.apk"
jarsigner -verify -verbose "${PROJECT_DIR:-.}/build/android/app/outputs/bundle/release/app-release.aab"
```

## debug の鍵で署名したリリースビルド (releaseDebugSigned)

リリース構成 (proguard / minify などが効いた状態) を、**リリース用の鍵を用意せずに debug の鍵で署名**して
試すためのビルドタイプ `releaseDebugSigned` があります。

- リリース構成での動作や速さを手元の端末で確かめたい
- まだリリース用の鍵が無い / CI に署名情報を渡せない段階でも、リリース構成の APK を作りたい
- Play に出すのではなく、社内配布や動作確認用の APK がほしい

`app/build.gradle` の `buildTypes.releaseDebugSigned` は release を `initWith` したうえで、`signingConfig` を
`signingConfigs.debug` に差し替えたものです。`local.properties` の `RELEASE_*` は要りません。

```bash
make release-debug           # APK を作る
make install-release-debug   # 端末に入れる

# Gradle を直接呼ぶ場合
./gradlew :app:assembleReleaseDebugSigned
./gradlew :app:installReleaseDebugSigned
```

出力先は `${PROJECT_DIR}/build/android/app/outputs/apk/releaseDebugSigned/` です。

> debug の鍵で署名されるので、**Google Play にはアップロードできません**。動作確認専用です。
> Google Play Games Services を使う場合は、debug キーストアの SHA-1 も Play Console に登録しておく必要が
> あります ([Play Games](android-play-games.md) を参照)。

## Play App Signing

Google Play Console は「Play App Signing」を推奨しています。使うと、

- Google がアップロード用の鍵とは別の署名鍵でアプリに署名し直します
- アップロード用の鍵を失くしても、Google に頼んで差し替えられます

最初のアップロードのときに Play App Signing への登録を選べます。

## CI での署名

GitHub Actions などでは、秘密情報を Secrets に登録して環境変数で渡します。

```yaml
env:
  RELEASE_STORE_PASSWORD: ${{ secrets.RELEASE_STORE_PASSWORD }}
  RELEASE_KEY_ALIAS: ${{ secrets.RELEASE_KEY_ALIAS }}
  RELEASE_KEY_PASSWORD: ${{ secrets.RELEASE_KEY_PASSWORD }}
```

キーストアは Base64 にして Secret に入れ、ビルド時に戻します。

```yaml
- name: Decode Keystore
  run: echo "${{ secrets.RELEASE_KEYSTORE_BASE64 }}" | base64 -d > release-key.jks
```
