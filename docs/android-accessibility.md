# 読み上げ (TalkBack) 対応

吉里吉里Z の読み上げ対応 (Elements のダイアログ、`ElementsDialog.setGameA11y` /
`a11yLayers` で載せたノード、`ElementsDialog.announce`) を Android の TalkBack へ出す。
スクリプト側の使い方は吉里吉里Z の
[読み上げ (スクリーンリーダー)](https://github.com/wamsoft/krkrz_dev/blob/develop/doc/guide/Accessibility.md)
を参照。ここは Android でのビルドと仕組みだけを書く。

## 有効 / 無効

既定で有効。同梱の `app-config.json` は `cmake.plugins` と `cmake.staticPlugins` に
`"a11y"` を入れてある。自分の設定ファイルを使う場合も同じように入れる。

```json
"cmake": {
    "plugins":       [ ..., "a11y" ],
    "staticPlugins": [ ..., "a11y" ]
}
```

`app/build.gradle` がこれを見て、エンジンを `-DKRKRZ_USE_A11Y=ON` でビルドする
(Play Games と同じく、プラグインの有無が唯一のスイッチ)。**無効にするには両方のリストから
`"a11y"` を外す** (`-DKRKRZ_USE_A11Y=OFF` になる)。外しても OS へ出なくなるだけで、
スクリプトの API (`announce` など) はそのまま使える。サイズの増分は `libkrkrz.so` が ABI ごとに
約 0.6〜0.7 MB (arm64-v8a で +4.3%)。

## 仕組み

| 場所 | 内容 |
|---|---|
| `app/src/main/java/jp/wamsoft/krkrz/a11y/A11ySurface.java` | SDL の描画面 (`SDLSurface`) のサブクラス。`getAccessibilityNodeProvider()` と `onHoverEvent()` (TalkBack のなぞり操作) を native へ渡す。`KrkrzActivity.createSDLSurface()` が、読み上げ入りのビルドのときだけこれを返す |
| `app/src/main/java/jp/wamsoft/krkrz/a11y/A11yBridge.java` | native メソッドの置き場。`AccessibilityManager` で TalkBack の on / off を native へ伝える。エンジン側の変化は `requestUpdate()` (native から呼ばれる) で UI スレッドへ回す |
| `app/src/main/java/jp/wamsoft/krkrz/a11y/A11yFocusOverlay.java` | TalkBack のフォーカス枠を描く透明な View。描画面 (SurfaceView) はウィンドウの背面にあり、その範囲はウィンドウ側では透明扱いなので、Android が描く枠は見えない。描画面の上に重ねて、TalkBack と同じ色・太さで枠を描く。TalkBack が初めてフォーカスを置いたときに付ける (起動中に View を足すと描画面のレイアウトが走り、SDL のウィンドウ生成とぶつかって固まる / GL の初期化が落ちることがあったため) |
| `app/src/main/cpp/a11y/` | 静的プラグイン。JNI 関数をエンジンの `TVPA11yAndroid*` (krkrz_dev `src/core/sdl3/environ/A11yAndroid.cpp`) へ渡すだけ |

- Android のアクセシビリティは UI スレッドで View に問い合わせる。エンジンは SDL のスレッドで
  動くので、ツリーはエンジンが最後に送ったものを答え、TalkBack からの操作 (ダブルタップ、
  スライダーの増減) はエンジンのスレッドへ積んで次のフレームで処理する。
- TalkBack のスワイプ巡回は、画面に見えるものにだけ止まるよう A11ySurface が調整する: 位置の無い
  ノードは見えない扱い、エンジンが announce 用に置く 1 ピクセルの live region は重要でない扱い
  (読み上げは届く)、根 (画面全体) の名前は画面の題 (pane title) に移して止まらせない。読ませたい
  ノードには位置 (`rect`) を付けること。一周の端で鳴る「ぷっ」は TalkBack の効果音。
- プラグインを入れていないビルドでは native が無いので、`A11yBridge.init()` が false になり、
  通常の `SDLSurface` のまま動く (Java は常にコンパイルされる)。
- TalkBack が動いている間、画面へのタッチは TalkBack の操作になり、ゲームには直接届かない
  (OS の仕様)。タッチで遊ぶ部分は `setGameA11y` / `a11yLayers` で操作できるようにしておく。

## 確かめ方

- `adb shell uiautomator dump /sdcard/ui.xml && adb pull /sdcard/ui.xml` で、
  描画面の下に仮想ノード (ボタンの名前・矩形) が出ているかを見る。
- TalkBack: 端末の設定から有効にする。adb からは
  `adb shell settings put secure enabled_accessibility_services com.google.android.marvin.talkback/com.google.android.marvin.talkback.TalkBackService`
  と `adb shell settings put secure accessibility_enabled 1` (戻すときは
  `enabled_accessibility_services` を空にする)。
- REPL (`make repl`、[android-repl.md](android-repl.md)) で `Agent.a11yTree()` を評価して、
  エンジンが持っているツリーと突き合わせる (socket の REPL は `.a11y` などのドット
  コマンドを受け付けないので、式で書く)。`ElementsDialog.a11yActive` は TalkBack が
  最初にツリーを問い合わせた時点で真になる。

## 既知の問題

- TalkBack を切ったときに `ElementsDialog.a11yActive` が偽へ戻るかは、実機で未確認。
- 左端スワイプのサイドメニューは、TalkBack 中は 2 本指で左端からスワイプすると開く
  (1 本指のスワイプは TalkBack の操作になる)。
- 画面が消えている間にアプリを起動すると、ウィンドウ生成と描画面の変化がぶつかって起動時に
  固まることがある (SDL 側の起動タイミングの問題。読み上げとは別)。
