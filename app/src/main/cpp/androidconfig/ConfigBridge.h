// androidconfig プラグイン - Java 設定 UI と TJS グローバル設定関数の橋渡し。
//
// 役割:
//   - Java (設定画面) から呼ばれる JNI 関数で「コンフィグ取得」「コンフィグ適用」
//     「音量設定」をリクエストとしてキューに積む。
//   - キューは tTVPContinuousEventCallbackIntf 経由で **TJS メインスレッド** 上で
//     drain される (drainRequests)。TJS オブジェクト生成・グローバル関数呼び出しは
//     すべてこの drain 内 = TJS スレッドで行い、スレッド安全性を担保する。
//   - 呼び出す TJS 側グローバル関数 (存在すれば呼ぶ):
//       KrkrzAndroidGetConfig()            -> Dictionary
//       KrkrzAndroidSetConfig(dict)        -> void
//       KrkrzAndroidGetVolume(key)         -> int (0-100)
//       KrkrzAndroidSetVolume(key, value)  -> void
//
// JNI_OnLoad はここでは定義しない (SDL / playgames 等と衝突させないため、かつ不要)。
// JavaVM は androidconfig 自身の JNI 呼び出しに渡る env から env->GetJavaVM で取得・
// キャッシュするので、playgames プラグインの有無 (app-config.json で外せる) に依存しない。

#pragma once

#include <jni.h>

namespace androidconfig {

// TJS スレッド (continuous event hook) から呼ばれ、キュー済みリクエストを処理する。
void drainRequests();

// プラグイン解放時にキュー・グローバル参照を片付ける。
void shutdown();

} // namespace androidconfig
