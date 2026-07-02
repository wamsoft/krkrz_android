# PlayGames プラグイン TJS 使い方ガイド

KrkrZ (TJS) スクリプトから Google Play Games Services を使う具体例集。

- API リファレンス: 同フォルダの `manual.tjs`
- Play Console 側のセットアップ / 失敗時の調査: 同フォルダの `SETUP.md`
- Android 側の動作確認 UI: アプリ起動 → 左端スワイプ →「Google Play Games」
  (サインイン状態 / 実績一覧 / Snapshots 一覧を読み出して表示する管理画面)

このドキュメントは「**TJS から実績を解除したい / クラウドセーブを読み書きしたい**」
というよくあるユースケースを、コピペで動くスニペット形式でまとめている。

---

## 1. 用意するもの (最低限)

1. Play Console で **App ID** を発行 → `app/src/main/res/values/strings.xml`
   の `games_app_id` を実 ID に書き換え (SETUP.md §1-1, §2-1)
2. Play Console で **実績** を作成 → 発行された `CgkI...` 形式の ID を控えておく
   (実績を使う場合のみ)
3. Play Console で **「保存されたゲーム」** を有効化 (Snapshots を使う場合のみ)
4. テスト用 Google アカウントを Play Console の「テスター」に追加 (SETUP.md §1-4)

このプラグインは `KrkrzActivity.onCreate` で自動初期化される (`PlayGamesBridge.initialize`)
ため、TJS 側で初期化呼び出しは不要。`PlayGames` クラスがそのまま使える。

---

## 2. 共通の呼び出しパターン

すべての非同期メソッドは **第 1 引数に function (callback)** を取り、結果を
`callback(result, error)` の形で受ける。

- 成功時: `result` に値、`error` は `void`
- 失敗時: `result` は `void`、`error` に文字列のエラーメッセージ

```tjs
// 共通のエラーハンドラ
function pgHandle(result, error, onSuccess) {
    if (error !== void) {
        Debug.message("[PlayGames] " + error);
        return;
    }
    if (onSuccess !== void) onSuccess(result);
}
```

`callback` を **省略 (void を渡す)** すると **fire-and-forget**。
解除しっぱなしで結果を待たない実績解除などに便利。

```tjs
PlayGames.unlockAchievement(void, "CgkI...");  // ← 結果無視
```

---

## 3. 認証 (sign-in)

Play Games SDK v2 は **アプリ起動時に自動 silent sign-in** を試みる。
通常は `isAuthenticated` で確認するだけで OK。
手動 sign-in は **ユーザの明示的操作** (例: 設定画面の「サインイン」ボタン) に限るのが
ガイドライン的にも推奨。

### 3-1. 起動時に状態だけ確認

```tjs
PlayGames.isAuthenticated(function(authed, err) {
    if (err !== void) {
        Debug.message("[PlayGames] " + err);
        return;
    }
    if (authed) {
        // すでにサインイン済み。実績ロードや snapshots チェックなど開始
        loadUserGameState();
    } else {
        // サインインしていない。実績は呼ばないか、ローカルだけで動く
        Debug.message("[PlayGames] not signed in (skip cloud ops)");
    }
});
```

### 3-2. プレイヤー名を取りたい

```tjs
PlayGames.getCurrentPlayer(function(info, err) {
    if (err !== void) return;
    Debug.message("welcome, " + info.displayName);  // または info.playerId
});
```

### 3-3. ユーザ操作でサインイン (任意)

```tjs
// 例えば「設定」画面の「Google Play Games にサインイン」ボタン
function onSignInButton() {
    PlayGames.signIn(function(ok, err) {
        if (err !== void) { Debug.message("sign-in failed: " + err); return; }
        if (ok) Debug.message("signed in");
    });
}
```

---

## 4. 実績 (Achievements)

実績 ID は **Play Console で実績を作成すると発行される `CgkI...` で始まる文字列**。
TJS スクリプトの中では定数として持っておくと管理しやすい。

```tjs
// 例: チャプタクリア実績の ID テーブル
class AchievementIds {
    var clearChapter1 = "CgkI__________EQAQ";
    var clearChapter2 = "CgkI__________EQAg";
    var allEndingsSeen = "CgkI__________EQAw";
    var read10kLines  = "CgkI__________EQBA";  // 段階的実績 (10000 行読破)
}
global.AchIds = new AchievementIds();
```

### 4-1. 通常 (standard) 実績の解除

最も単純な fire-and-forget:

```tjs
// チャプタ 1 をクリアしたタイミングで:
PlayGames.unlockAchievement(void, AchIds.clearChapter1);
```

結果を確認したい場合:

```tjs
PlayGames.unlockAchievement(function(ok, err) {
    if (err !== void) { Debug.message("unlock failed: " + err); return; }
    // ok = true (Play Games SDK 側で重複解除も受け流すので「既に解除済み」も成功扱い)
}, AchIds.clearChapter1);
```

> ヒント: Play Games SDK は **同じ実績の重複解除呼び出しを安全に処理** する。
> 「初めてクリアか」をスクリプト側で覚えて分岐する必要は無い。毎回呼んで良い。

### 4-2. 段階的 (incremental) 実績

「累計 10000 行読破」など、複数回呼んでカウントアップする実績。
**`unlockAchievement` ではなく `incrementAchievement` を使う** (常識的な勘違いに注意)。
totalSteps は Play Console 側で実績を作成するときに設定する。

加算型 (例: 1 メッセージ読むごとに +1):

```tjs
// メッセージ表示時のイベントハンドラから
PlayGames.incrementAchievement(void, AchIds.read10kLines, 1);
```

絶対値設定型 (例: セーブデータの累計値で同期):

```tjs
// セーブデータから読んだ累計行数で setSteps:
PlayGames.setStepsAchievement(void, AchIds.read10kLines, savedata.totalLinesRead);
```

両者の違い:
- `increment(cb, id, delta)` — 現在値 + delta。シンプルだが端末をまたぐと多重カウント
  しやすい (端末 A で 100、端末 B で 100 → サーバ的に 200 になる可能性)
- `setSteps(cb, id, abs)` — 現在値が `abs` より小さければ `abs` に。**冪等**。
  セーブデータの累計値と同期するときに安全

ノベルゲームでよくあるユースケースは setSteps の方が破綻しにくい。

### 4-3. 起動時に解除済みの実績を確認 (UI 反映用)

ゲーム内に「自前の実績一覧画面」を出したい場合、最新状態をサーバから取って表示:

```tjs
PlayGames.loadAchievements(function(list, err) {
    if (err !== void) return;
    // list は配列、各要素は dictionary
    for (var i = 0; i < list.count; i++) {
        var a = list[i];
        Debug.message(a.name + " : " +
            (a.state == 0 ? "unlocked" :
             a.state == 1 ? "revealed" : "hidden"));
        if (a.type == 1) {  // incremental
            Debug.message("  progress: " + a.currentSteps + "/" + a.totalSteps);
        }
    }
}, true);  // forceReload=true で必ずサーバから再取得
```

### 4-4. Play Games 純正の実績一覧UIを呼ぶ

自前 UI を持たない場合は SDK 提供の Activity を起動するだけで完結:

```tjs
// メニュー → 「実績」ボタンから
PlayGames.showAchievementsUI(function(ok, err) {
    if (err !== void) Debug.message("UI launch failed: " + err);
});
```

---

## 5. セーブゲーム (Snapshots)

Snapshots は **「名前付きスロット (uniqueName) に任意バイト列を保存」** する仕組み。
端末 A で保存 → 端末 B でロードできる **クラウドセーブ** として使う。

### 5-1. データ形式

`writeSnapshot` の `data` 引数は **octet (バイナリ) または string**。
TJS のオブジェクト/辞書をそのまま渡せないので、シリアライズしてから渡す。

#### パターン A: 文字列保存 (人が読める / 小規模データ)

```tjs
// TJS 標準の repr 形式でシリアライズして文字列で保存
var savedata = %[
    chapter: 3,
    flags: %[ saw_event_01: true, gotItem_red: true ],
    position: "garden_morning"
];
var serialized = savedata.saveStruct();  // KAG 標準ヘルパー、JSON っぽいテキスト

PlayGames.writeSnapshot(function(ok, err) {
    if (err !== void) { Debug.message("save failed: " + err); return; }
    Debug.message("cloud save ok");
},
    "slot01",                          // uniqueName
    serialized,                        // data (string)
    "Chapter 3 — 庭にて (朝)",          // description (Play Games UIに出る)
    System.getTickCount(),             // playedTimeMs (累計プレイ時間ms)
    /*progressValue=*/30               // 進捗値 (例: ストーリー進行率 0-100)
);
```

#### パターン B: バイナリ保存 (大規模 / 既存セーブ形式と互換)

```tjs
// 例: 自前のセーブシステムが Storages.outputBytes で出力する .ksf 形式
var data = Storages.readBytes(Storages.getLocalName("save01.ksf"));
PlayGames.writeSnapshot(function(ok, err) {
    Debug.message(ok ? "cloud save ok" : err);
}, "slot01", data, "...説明...");
```

### 5-2. 読み込み

```tjs
PlayGames.readSnapshot(function(result, err) {
    if (err !== void) {
        // 初回起動など、まだ保存されていない場合もここに来る
        Debug.message("[PlayGames] no cloud save: " + err);
        return;
    }
    // result.metadata : %[ uniqueName, description, lastModifiedTimestamp, ... ]
    // result.data     : octet (バイト列)
    var data = result.data;
    // string で保存した場合は文字列化して loadStruct で復元
    var serialized = (string)data;
    var savedata = Dictionary.loadStruct(serialized);
    Debug.message("loaded: chapter=" + savedata.chapter);
}, "slot01");
```

### 5-3. 一覧取得 (自前 UI を出すとき)

```tjs
PlayGames.listSnapshots(function(list, err) {
    if (err !== void) return;
    for (var i = 0; i < list.count; i++) {
        var m = list[i];
        Debug.message(m.uniqueName + " | " + m.description +
                      " | " + new Date(m.lastModifiedTimestamp).toString());
    }
}, true);  // forceReload=true で必ずサーバから再取得
```

### 5-4. 削除

```tjs
PlayGames.deleteSnapshot(function(ok, err) {
    Debug.message(ok ? "deleted" : err);
}, "slot01");
```

### 5-5. Play Games 純正のセーブ選択UI

```tjs
PlayGames.showSnapshotsUI(function(ok, err) {
    if (err !== void) return;
    // UI から戻った後で listSnapshots を呼び直して最新状態を取り、
    // 選択結果をスクリプト内で照合するのが現状の運用
    // (純正UIの「選択結果コールバック」は SETUP.md §6-1 で TODO)
},
    "セーブを選択",                      // title
    /*allowAddBtn=*/true,
    /*allowDeleteBtn=*/true,
    /*maxSnapshots=*/0                 // 0 = 制限なし
);
```

### 5-6. 競合解決 (同期の落とし穴)

- スロット名 (`uniqueName`) は **端末をまたいで同じ値** にする (例: "main", "slot01")
- 両端末で同名スロットに保存した場合、自動で **「最後に更新された方」が採用** される
  (`RESOLUTION_POLICY_MOST_RECENTLY_MODIFIED` 固定)
- 端末をまたぐような運用ではセーブごとに `lastModifiedTimestamp` をチェックして
  「サーバ側の方が新しければそちらを採用」みたいなロジックを TJS 側で書くと安全

```tjs
// パターン: 起動時に「クラウド側がローカル側より新しければクラウドを採用」
function syncFromCloudIfNewer() {
    PlayGames.listSnapshots(function(list, err) {
        if (err !== void || list.count == 0) return;
        for (var i = 0; i < list.count; i++) {
            if (list[i].uniqueName == "main") {
                var cloudTs = list[i].lastModifiedTimestamp;
                var localTs = getLocalSaveModifiedTime();  // 自前関数
                if (cloudTs > localTs) {
                    Debug.message("cloud save is newer, pulling...");
                    PlayGames.readSnapshot(function(r, e) {
                        if (e === void) applyCloudSave(r.data);
                    }, "main");
                }
                return;
            }
        }
    }, true);
}
```

---

## 6. よくあるレシピ

### 6-1. ゲーム起動時の典型フロー

```tjs
// startup.tjs などから
function initPlayGames() {
    PlayGames.isAuthenticated(function(authed, err) {
        if (err !== void || !authed) {
            Debug.message("PlayGames: offline (no cloud features)");
            global.PlayGamesEnabled = false;
            return;
        }
        global.PlayGamesEnabled = true;
        // クラウドセーブが新しければ取り込む
        syncFromCloudIfNewer();
        // 解除済み実績の状態を取得して UI 用にキャッシュ
        PlayGames.loadAchievements(function(list, e) {
            if (e === void) cacheAchievementStates(list);
        });
    });
}
initPlayGames();
```

### 6-2. 章クリアで実績解除

```tjs
function onChapterCleared(chapterNum) {
    if (!global.PlayGamesEnabled) return;
    var id = void;
    if      (chapterNum == 1) id = AchIds.clearChapter1;
    else if (chapterNum == 2) id = AchIds.clearChapter2;
    if (id !== void) {
        PlayGames.unlockAchievement(void, id);   // fire-and-forget
    }
}
```

### 6-3. オートセーブ (5 分ごとにクラウドへ)

```tjs
class CloudAutoSave {
    var timer;
    function CloudAutoSave() {
        timer = new Timer(this.tick, "");
        timer.interval = 5 * 60 * 1000;
        timer.enabled = true;
    }
    function tick() {
        if (!global.PlayGamesEnabled) return;
        var snapshot = collectCurrentGameState().saveStruct();
        PlayGames.writeSnapshot(function(ok, err) {
            if (err !== void) Debug.message("autosave failed: " + err);
        }, "auto", snapshot, "Auto " + new Date().toString(),
           System.getTickCount(), getCurrentProgress());
    }
}
global.autoSave = new CloudAutoSave();
```

### 6-4. 手動セーブ画面で純正UIを出す

```tjs
// セーブメニューで「クラウドセーブを開く」ボタンから
PlayGames.showSnapshotsUI(void, "セーブ選択", true, true, 0);
// 戻った後は別タイミングで listSnapshots / readSnapshot を呼ぶ
```

---

## 7. デバッグ / トラブルシューティング

- **`make logcat | grep -E "playgames|PlayGames|GamesSignIn"`** で SDK 側ログを観察
- アプリ内の **管理画面 (左端スワイプ → Google Play Games)** で
  サインイン状態 / 実績一覧 / Snapshots 一覧を Java SDK 経由で確認できる。
  TJS 側 (`PlayGames.loadAchievements` 等) の結果と齟齬があれば、コールバック側の
  エラー処理を見直す
- エラー番号別の対処は `SETUP.md` §5「トラブルシューティング」を参照

---

## 8. 関連ファイル

- TJS API 宣言 (この一覧): `manual.tjs`
- C++ 実装: `Main.cpp` (NCB 登録) + `PlayGamesAuth.cpp` / `PlayGamesAchievements.cpp` / `PlayGamesSnapshots.cpp` (各サブシステム)
- Java 実装: `app/src/playgames/java/jp/wamsoft/krkrz/playgames/PlayGames*.java` (有効ビルドのみコンパイル)
- Android 管理画面: `app/src/playgames/java/jp/wamsoft/krkrz/ui/PlayGamesAdminActivity.java`
- 入口ドキュメント (有効/無効の仕組み含む): `docs/android-play-games.md`
- セットアップ手順: `SETUP.md`
