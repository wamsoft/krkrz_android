#include "PlayGamesSnapshots.h"
#include "JniBridge.h"

namespace playgames {

tjs_error CallListSnapshots(const tTJSVariantClosure &cb, bool forceReload) {
    ScopedEnv se;
    if (!se.ok()) return TJS_E_FAIL;
    JNIEnv *env = se.env();
    jclass cls = getClassCache(env)->clsSnapshots;
    if (!cls) return TJS_E_FAIL;
    jmethodID mid = env->GetStaticMethodID(cls, "list", "(JZ)V");
    if (!mid) { if (env->ExceptionCheck()) env->ExceptionClear(); return TJS_E_FAIL; }
    tjs_int64 cbid = registerCallback(cb);
    env->CallStaticVoidMethod(cls, mid, (jlong)cbid, (jboolean)forceReload);
    if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); return TJS_E_FAIL; }
    return TJS_S_OK;
}

tjs_error CallReadSnapshot(const tTJSVariantClosure &cb, const ttstr &uniqueName) {
    ScopedEnv se;
    if (!se.ok()) return TJS_E_FAIL;
    JNIEnv *env = se.env();
    jclass cls = getClassCache(env)->clsSnapshots;
    if (!cls) return TJS_E_FAIL;
    jmethodID mid = env->GetStaticMethodID(cls, "read", "(JLjava/lang/String;)V");
    if (!mid) { if (env->ExceptionCheck()) env->ExceptionClear(); return TJS_E_FAIL; }
    tjs_int64 cbid = registerCallback(cb);
    jstring jname = ttstrToJString(env, uniqueName);
    env->CallStaticVoidMethod(cls, mid, (jlong)cbid, jname);
    if (jname) env->DeleteLocalRef(jname);
    if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); return TJS_E_FAIL; }
    return TJS_S_OK;
}

tjs_error CallWriteSnapshot(const tTJSVariantClosure &cb,
                              const ttstr &uniqueName,
                              const tTJSVariant &data,
                              const ttstr &description,
                              tjs_int64 playedTimeMs,
                              tjs_int64 progressValue) {
    ScopedEnv se;
    if (!se.ok()) return TJS_E_FAIL;
    JNIEnv *env = se.env();
    jclass cls = getClassCache(env)->clsSnapshots;
    if (!cls) return TJS_E_FAIL;
    jmethodID mid = env->GetStaticMethodID(cls, "write", "(JLjava/lang/String;[BLjava/lang/String;JJ)V");
    if (!mid) { if (env->ExceptionCheck()) env->ExceptionClear(); return TJS_E_FAIL; }

    // data を byte[] に変換: octet そのまま、string なら UTF-8 化
    jbyteArray jdata = nullptr;
    if (data.Type() == tvtOctet) {
        tTJSVariantOctet *o = data.AsOctetNoAddRef();
        if (o) {
            tjs_uint len = o->GetLength();
            jdata = env->NewByteArray((jsize)len);
            if (jdata && len > 0)
                env->SetByteArrayRegion(jdata, 0, len,
                    reinterpret_cast<const jbyte*>(o->GetData()));
        }
    } else if (data.Type() == tvtString) {
        ttstr s = data.AsStringNoAddRef();
        int maxlen = (int)s.length() * 6 + 1;
        std::vector<char> buf(maxlen);
        int len = TVPWideCharToUtf8String(s.c_str(), buf.data());
        jdata = env->NewByteArray((jsize)len);
        if (jdata && len > 0)
            env->SetByteArrayRegion(jdata, 0, len, reinterpret_cast<const jbyte*>(buf.data()));
    } else {
        jdata = env->NewByteArray(0);
    }

    tjs_int64 cbid = registerCallback(cb);
    jstring jname = ttstrToJString(env, uniqueName);
    jstring jdesc = description.IsEmpty() ? nullptr : ttstrToJString(env, description);

    env->CallStaticVoidMethod(cls, mid,
        (jlong)cbid, jname, jdata, jdesc,
        (jlong)playedTimeMs, (jlong)progressValue);

    if (jname) env->DeleteLocalRef(jname);
    if (jdesc) env->DeleteLocalRef(jdesc);
    if (jdata) env->DeleteLocalRef(jdata);

    if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); return TJS_E_FAIL; }
    return TJS_S_OK;
}

tjs_error CallDeleteSnapshot(const tTJSVariantClosure &cb, const ttstr &uniqueName) {
    ScopedEnv se;
    if (!se.ok()) return TJS_E_FAIL;
    JNIEnv *env = se.env();
    jclass cls = getClassCache(env)->clsSnapshots;
    if (!cls) return TJS_E_FAIL;
    jmethodID mid = env->GetStaticMethodID(cls, "delete", "(JLjava/lang/String;)V");
    if (!mid) { if (env->ExceptionCheck()) env->ExceptionClear(); return TJS_E_FAIL; }
    tjs_int64 cbid = registerCallback(cb);
    jstring jname = ttstrToJString(env, uniqueName);
    env->CallStaticVoidMethod(cls, mid, (jlong)cbid, jname);
    if (jname) env->DeleteLocalRef(jname);
    if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); return TJS_E_FAIL; }
    return TJS_S_OK;
}

tjs_error CallShowSnapshotsUI(const tTJSVariantClosure &cb,
                                const ttstr &title,
                                bool allowAddBtn,
                                bool allowDeleteBtn,
                                tjs_int maxSnapshots) {
    ScopedEnv se;
    if (!se.ok()) return TJS_E_FAIL;
    JNIEnv *env = se.env();
    jclass cls = getClassCache(env)->clsSnapshots;
    if (!cls) return TJS_E_FAIL;
    jmethodID mid = env->GetStaticMethodID(cls, "showUI", "(JLjava/lang/String;ZZI)V");
    if (!mid) { if (env->ExceptionCheck()) env->ExceptionClear(); return TJS_E_FAIL; }
    tjs_int64 cbid = registerCallback(cb);
    jstring jtitle = ttstrToJString(env, title);
    env->CallStaticVoidMethod(cls, mid, (jlong)cbid, jtitle,
        (jboolean)allowAddBtn, (jboolean)allowDeleteBtn, (jint)maxSnapshots);
    if (jtitle) env->DeleteLocalRef(jtitle);
    if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); return TJS_E_FAIL; }
    return TJS_S_OK;
}

} // namespace playgames
