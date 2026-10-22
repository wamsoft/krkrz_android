// a11y JNI bridge - Java の A11yBridge の native メソッドを、エンジン本体の
// TVPA11yAndroid* (src/core/sdl3/environ/A11yAndroid.cpp) へそのまま渡す。
// どれも Android の UI スレッドから呼ばれる。

#include <jni.h>

// エンジン本体で定義 (KRKRZ_USE_A11Y=ON のビルドのみ)
extern "C" {
void     TVPA11yAndroidRegister(JNIEnv* env, jclass bridge);
jobject  TVPA11yAndroidCreateNodeInfo(JNIEnv* env, jobject host, jint virtualViewId);
jobject  TVPA11yAndroidFindFocus(JNIEnv* env, jobject host, jint focusType);
jboolean TVPA11yAndroidPerformAction(JNIEnv* env, jobject host, jint virtualViewId, jint action, jobject arguments);
jboolean TVPA11yAndroidOnHover(JNIEnv* env, jobject host, jint action, jfloat x, jfloat y);
void     TVPA11yAndroidUpdate(JNIEnv* env, jobject host);
void     TVPA11yAndroidSetEnabled(jboolean enabled);
}

// 静的プラグイン用エントリ関数 (TJS クラス登録は無いので no-op。license と同じ)
extern "C" void krkrz_plugin_a11y() {
}

extern "C" {

JNIEXPORT void JNICALL
Java_jp_wamsoft_krkrz_a11y_A11yBridge_nativeRegister(JNIEnv* env, jclass cls) {
    TVPA11yAndroidRegister(env, cls);
}

JNIEXPORT jobject JNICALL
Java_jp_wamsoft_krkrz_a11y_A11yBridge_nativeCreateNodeInfo(
        JNIEnv* env, jclass, jobject host, jint virtualViewId) {
    return TVPA11yAndroidCreateNodeInfo(env, host, virtualViewId);
}

JNIEXPORT jobject JNICALL
Java_jp_wamsoft_krkrz_a11y_A11yBridge_nativeFindFocus(
        JNIEnv* env, jclass, jobject host, jint focusType) {
    return TVPA11yAndroidFindFocus(env, host, focusType);
}

JNIEXPORT jboolean JNICALL
Java_jp_wamsoft_krkrz_a11y_A11yBridge_nativePerformAction(
        JNIEnv* env, jclass, jobject host, jint virtualViewId, jint action, jobject arguments) {
    return TVPA11yAndroidPerformAction(env, host, virtualViewId, action, arguments);
}

JNIEXPORT jboolean JNICALL
Java_jp_wamsoft_krkrz_a11y_A11yBridge_nativeOnHover(
        JNIEnv* env, jclass, jobject host, jint action, jfloat x, jfloat y) {
    return TVPA11yAndroidOnHover(env, host, action, x, y);
}

JNIEXPORT void JNICALL
Java_jp_wamsoft_krkrz_a11y_A11yBridge_nativeUpdate(JNIEnv* env, jclass, jobject host) {
    TVPA11yAndroidUpdate(env, host);
}

JNIEXPORT void JNICALL
Java_jp_wamsoft_krkrz_a11y_A11yBridge_nativeSetEnabled(JNIEnv*, jclass, jboolean enabled) {
    TVPA11yAndroidSetEnabled(enabled);
}

} // extern "C"
