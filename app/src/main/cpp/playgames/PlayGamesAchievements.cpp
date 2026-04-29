#include "PlayGamesAchievements.h"
#include "JniBridge.h"

namespace playgames {

tjs_error CallLoadAchievements(const tTJSVariantClosure &cb, bool forceReload) {
    ScopedEnv se;
    if (!se.ok()) return TJS_E_FAIL;
    JNIEnv *env = se.env();
    jclass cls = getClassCache(env)->clsAchievements;
    if (!cls) return TJS_E_FAIL;
    jmethodID mid = env->GetStaticMethodID(cls, "load", "(JZ)V");
    if (!mid) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return TJS_E_FAIL;
    }
    tjs_int64 cbid = registerCallback(cb);
    env->CallStaticVoidMethod(cls, mid, (jlong)cbid, (jboolean)forceReload);
    if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); return TJS_E_FAIL; }
    return TJS_S_OK;
}

tjs_error CallUnlockAchievement(const tTJSVariantClosure &cb, const ttstr &achievementId) {
    ScopedEnv se;
    if (!se.ok()) return TJS_E_FAIL;
    JNIEnv *env = se.env();
    jclass cls = getClassCache(env)->clsAchievements;
    if (!cls) return TJS_E_FAIL;
    jmethodID mid = env->GetStaticMethodID(cls, "unlock", "(JLjava/lang/String;)V");
    if (!mid) { if (env->ExceptionCheck()) env->ExceptionClear(); return TJS_E_FAIL; }
    tjs_int64 cbid = registerCallback(cb);
    jstring jid = ttstrToJString(env, achievementId);
    env->CallStaticVoidMethod(cls, mid, (jlong)cbid, jid);
    if (jid) env->DeleteLocalRef(jid);
    if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); return TJS_E_FAIL; }
    return TJS_S_OK;
}

tjs_error CallIncrementAchievement(const tTJSVariantClosure &cb, const ttstr &achievementId, tjs_int numSteps) {
    ScopedEnv se;
    if (!se.ok()) return TJS_E_FAIL;
    JNIEnv *env = se.env();
    jclass cls = getClassCache(env)->clsAchievements;
    if (!cls) return TJS_E_FAIL;
    jmethodID mid = env->GetStaticMethodID(cls, "increment", "(JLjava/lang/String;I)V");
    if (!mid) { if (env->ExceptionCheck()) env->ExceptionClear(); return TJS_E_FAIL; }
    tjs_int64 cbid = registerCallback(cb);
    jstring jid = ttstrToJString(env, achievementId);
    env->CallStaticVoidMethod(cls, mid, (jlong)cbid, jid, (jint)numSteps);
    if (jid) env->DeleteLocalRef(jid);
    if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); return TJS_E_FAIL; }
    return TJS_S_OK;
}

tjs_error CallSetStepsAchievement(const tTJSVariantClosure &cb, const ttstr &achievementId, tjs_int numSteps) {
    ScopedEnv se;
    if (!se.ok()) return TJS_E_FAIL;
    JNIEnv *env = se.env();
    jclass cls = getClassCache(env)->clsAchievements;
    if (!cls) return TJS_E_FAIL;
    jmethodID mid = env->GetStaticMethodID(cls, "setSteps", "(JLjava/lang/String;I)V");
    if (!mid) { if (env->ExceptionCheck()) env->ExceptionClear(); return TJS_E_FAIL; }
    tjs_int64 cbid = registerCallback(cb);
    jstring jid = ttstrToJString(env, achievementId);
    env->CallStaticVoidMethod(cls, mid, (jlong)cbid, jid, (jint)numSteps);
    if (jid) env->DeleteLocalRef(jid);
    if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); return TJS_E_FAIL; }
    return TJS_S_OK;
}

tjs_error CallShowAchievementsUI(const tTJSVariantClosure &cb) {
    ScopedEnv se;
    if (!se.ok()) return TJS_E_FAIL;
    JNIEnv *env = se.env();
    jclass cls = getClassCache(env)->clsAchievements;
    if (!cls) return TJS_E_FAIL;
    jmethodID mid = env->GetStaticMethodID(cls, "show", "(J)V");
    if (!mid) { if (env->ExceptionCheck()) env->ExceptionClear(); return TJS_E_FAIL; }
    tjs_int64 cbid = registerCallback(cb);
    env->CallStaticVoidMethod(cls, mid, (jlong)cbid);
    if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); return TJS_E_FAIL; }
    return TJS_S_OK;
}

} // namespace playgames
