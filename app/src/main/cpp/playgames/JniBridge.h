#pragma once
//
// JniBridge - JavaVM 取得 / JNIEnv attach / 型変換 / コールバックキュー
//
// PlayGames 系プラグインから JNI 呼び出しを行うときの共通ユーティリティ。
// JavaVM は JNI_OnLoad で取得済みのものを利用するが、SDL3 経由で起動する
// プラグインの場合 JNI_OnLoad が確実に呼ばれているとは限らないため、
// 初回呼び出し時に getJavaVm() でフォールバック取得 (AttachCurrentThread) する。
//

#include <jni.h>
#include "tp_stub.h"
#include "ncbind.hpp"
#include <string>
#include <functional>

namespace playgames {

// JavaVM を取得 (JNI_OnLoad / SDL ヘルパーから登録される)
JavaVM* getJavaVm();
void    setJavaVm(JavaVM *vm);

// 現在のスレッド向けに JNIEnv を取得。必要なら AttachCurrentThread。
// 戻り値: アタッチした場合のみ true。デストラクタで自動デタッチ。
class ScopedEnv {
public:
    ScopedEnv();
    ~ScopedEnv();
    JNIEnv* env() const { return env_; }
    bool ok() const { return env_ != nullptr; }
private:
    JNIEnv *env_ = nullptr;
    bool    attached_ = false;
};

// グローバル参照キャッシュ。
// PlayGamesBridge / PlayGamesNative などのクラスを最初の呼び出し時にロードして
// グローバル参照として保持する。
struct ClassCache {
    jclass clsBridge        = nullptr;  // jp.wamsoft.krkrz.playgames.PlayGamesBridge
    jclass clsAuth          = nullptr;  // jp.wamsoft.krkrz.playgames.PlayGamesAuth
    jclass clsAchievements  = nullptr;  // jp.wamsoft.krkrz.playgames.PlayGamesAchievements
    jclass clsSnapshots     = nullptr;  // jp.wamsoft.krkrz.playgames.PlayGamesSnapshots
};
ClassCache* getClassCache(JNIEnv *env);
void        clearClassCache(JNIEnv *env);

// jstring <-> ttstr (UTF-8 経由)
jstring ttstrToJString(JNIEnv *env, const ttstr &s);
ttstr   jStringToTtstr(JNIEnv *env, jstring js);

// jobject -> tTJSVariant の汎用変換。
//   null            -> void
//   Boolean         -> bool (integer)
//   Integer/Long    -> integer
//   Float/Double    -> real
//   String          -> ttstr
//   byte[]          -> octet (TJS の string ではなく array of bytes として渡す)
//   Object[]        -> Array
//   Map<String,?>   -> Dictionary
//   その他           -> ttstr (toString())
tTJSVariant jobjectToVariant(JNIEnv *env, jobject obj);

// tTJSVariant -> jobject (主に書き込み用に使う最低限)。
//   void/null   -> null
//   bool/integer-> java.lang.Long
//   real        -> java.lang.Double
//   string      -> java.lang.String
//   octet       -> byte[]
jobject variantToJObject(JNIEnv *env, const tTJSVariant &v);

// ---------------------------------------------------------
// コールバックキュー
//
// Java 側 PlayGamesNative.nativePostResult / nativePostError から呼ばれた結果を
// スレッドセーフキューに積み、TJS 側の continuous event hook タイミングで
// 元の callback クロージャを呼び出す。
// ---------------------------------------------------------

// 新規コールバックを登録、ID を取得。
// callbackId == 0 は「コールバック不要」を意味する予約値。
tjs_int64 registerCallback(const tTJSVariantClosure &closure);

// Java 側からの結果通知 (内部で使用)。スレッド安全。
void postResult(tjs_int64 callbackId, const tTJSVariant &value);
void postError (tjs_int64 callbackId, const ttstr &message);

// continuous event hook 用ドレイン。
// TJS スレッドから1フレーム1回呼び出す。
void drainCallbacks();

// プラグイン unload 時にキューと登録を破棄。
void shutdownCallbacks();

} // namespace playgames
