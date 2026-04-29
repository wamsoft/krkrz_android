#include "PlayGamesAuth.h"
#include "JniBridge.h"

namespace playgames {

namespace {
// 共通呼び出し: Java の static void method(long callbackId) を呼び出すだけ
static tjs_error callVoidWithCallback(jclass cls, const char *method, const tTJSVariantClosure &cb) {
    ScopedEnv se;
    if (!se.ok()) return TJS_E_FAIL;
    JNIEnv *env = se.env();
    if (!cls) return TJS_E_FAIL;

    tjs_int64 cbid = registerCallback(cb);
    jmethodID mid = env->GetStaticMethodID(cls, method, "(J)V");
    if (!mid) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return TJS_E_FAIL;
    }
    env->CallStaticVoidMethod(cls, mid, (jlong)cbid);
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        return TJS_E_FAIL;
    }
    return TJS_S_OK;
}
}

tjs_error CallIsAuthenticated(const tTJSVariantClosure &cb) {
    ScopedEnv se;
    if (!se.ok()) return TJS_E_FAIL;
    return callVoidWithCallback(getClassCache(se.env())->clsAuth, "isAuthenticated", cb);
}

tjs_error CallSignIn(const tTJSVariantClosure &cb) {
    ScopedEnv se;
    if (!se.ok()) return TJS_E_FAIL;
    return callVoidWithCallback(getClassCache(se.env())->clsAuth, "signIn", cb);
}

tjs_error CallCurrentPlayer(const tTJSVariantClosure &cb) {
    ScopedEnv se;
    if (!se.ok()) return TJS_E_FAIL;
    return callVoidWithCallback(getClassCache(se.env())->clsAuth, "currentPlayer", cb);
}

} // namespace playgames
