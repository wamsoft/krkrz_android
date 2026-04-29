#include "JniBridge.h"

#include <android/log.h>
#include <mutex>
#include <deque>
#include <unordered_map>
#include <atomic>
#include <vector>

#define LOG_TAG "playgames"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)

namespace playgames {

// -------------------- JavaVM --------------------

static JavaVM *gJvm = nullptr;

void setJavaVm(JavaVM *vm) { gJvm = vm; }
JavaVM* getJavaVm() { return gJvm; }

ScopedEnv::ScopedEnv() {
    if (!gJvm) return;
    int st = gJvm->GetEnv(reinterpret_cast<void**>(&env_), JNI_VERSION_1_6);
    if (st == JNI_OK) {
        attached_ = false;
    } else if (st == JNI_EDETACHED) {
        if (gJvm->AttachCurrentThread(&env_, nullptr) == JNI_OK) {
            attached_ = true;
        } else {
            env_ = nullptr;
        }
    } else {
        env_ = nullptr;
    }
}
ScopedEnv::~ScopedEnv() {
    if (attached_ && gJvm) {
        gJvm->DetachCurrentThread();
    }
}

// -------------------- ClassCache --------------------

static ClassCache gCache;
static std::once_flag gCacheOnce;

static jclass loadGlobalClass(JNIEnv *env, const char *name) {
    jclass local = env->FindClass(name);
    if (!local) {
        LOGE("FindClass failed: %s", name);
        if (env->ExceptionCheck()) env->ExceptionClear();
        return nullptr;
    }
    jclass global = (jclass) env->NewGlobalRef(local);
    env->DeleteLocalRef(local);
    return global;
}

ClassCache* getClassCache(JNIEnv *env) {
    std::call_once(gCacheOnce, [env]() {
        gCache.clsBridge       = loadGlobalClass(env, "jp/wamsoft/krkrz/playgames/PlayGamesBridge");
        gCache.clsAuth         = loadGlobalClass(env, "jp/wamsoft/krkrz/playgames/PlayGamesAuth");
        gCache.clsAchievements = loadGlobalClass(env, "jp/wamsoft/krkrz/playgames/PlayGamesAchievements");
        gCache.clsSnapshots    = loadGlobalClass(env, "jp/wamsoft/krkrz/playgames/PlayGamesSnapshots");
    });
    return &gCache;
}

void clearClassCache(JNIEnv *env) {
    if (gCache.clsBridge)       env->DeleteGlobalRef(gCache.clsBridge);
    if (gCache.clsAuth)         env->DeleteGlobalRef(gCache.clsAuth);
    if (gCache.clsAchievements) env->DeleteGlobalRef(gCache.clsAchievements);
    if (gCache.clsSnapshots)    env->DeleteGlobalRef(gCache.clsSnapshots);
    gCache = ClassCache{};
}

// -------------------- string conversion --------------------

jstring ttstrToJString(JNIEnv *env, const ttstr &s) {
    if (s.IsEmpty()) return env->NewStringUTF("");
    int maxlen = (int)s.length() * 6 + 1;
    std::vector<char> buf(maxlen);
    int len = TVPWideCharToUtf8String(s.c_str(), buf.data());
    buf[len] = '\0';
    return env->NewStringUTF(buf.data());
}

ttstr jStringToTtstr(JNIEnv *env, jstring js) {
    if (!js) return ttstr();
    const char *utf = env->GetStringUTFChars(js, nullptr);
    if (!utf) return ttstr();
    size_t len = strlen(utf);
    std::vector<tjs_char> buf(len * 2 + 1);
    tjs_uint w = TVPUtf8ToWideCharString(utf, buf.data());
    ttstr r(buf.data(), w);
    env->ReleaseStringUTFChars(js, utf);
    return r;
}

// -------------------- variant conversion --------------------

namespace {
struct ConvCtx {
    jclass cBoolean = nullptr;
    jclass cInteger = nullptr;
    jclass cLong    = nullptr;
    jclass cFloat   = nullptr;
    jclass cDouble  = nullptr;
    jclass cString  = nullptr;
    jclass cMap     = nullptr;
    jclass cIterator= nullptr;
    jclass cMapEntry= nullptr;
    jclass cSet     = nullptr;
    jclass cByteArr = nullptr;  // byte[] のクラスは特別扱い (IsInstanceOf でなく GetObjectClass で判定)
    jmethodID midBoolValue = nullptr;
    jmethodID midIntValue  = nullptr;
    jmethodID midLongValue = nullptr;
    jmethodID midFloatValue= nullptr;
    jmethodID midDoubleValue = nullptr;
    jmethodID midToString = nullptr;
    jmethodID midMapEntrySet= nullptr;
    jmethodID midSetIterator= nullptr;
    jmethodID midItHasNext = nullptr;
    jmethodID midItNext   = nullptr;
    jmethodID midEntryKey = nullptr;
    jmethodID midEntryVal = nullptr;
    bool ready = false;
};
static thread_local ConvCtx tlsCtx;

static void prepCtx(JNIEnv *env) {
    if (tlsCtx.ready) return;
    tlsCtx.cBoolean = (jclass) env->NewGlobalRef(env->FindClass("java/lang/Boolean"));
    tlsCtx.cInteger = (jclass) env->NewGlobalRef(env->FindClass("java/lang/Integer"));
    tlsCtx.cLong    = (jclass) env->NewGlobalRef(env->FindClass("java/lang/Long"));
    tlsCtx.cFloat   = (jclass) env->NewGlobalRef(env->FindClass("java/lang/Float"));
    tlsCtx.cDouble  = (jclass) env->NewGlobalRef(env->FindClass("java/lang/Double"));
    tlsCtx.cString  = (jclass) env->NewGlobalRef(env->FindClass("java/lang/String"));
    tlsCtx.cMap     = (jclass) env->NewGlobalRef(env->FindClass("java/util/Map"));
    tlsCtx.cIterator= (jclass) env->NewGlobalRef(env->FindClass("java/util/Iterator"));
    tlsCtx.cMapEntry= (jclass) env->NewGlobalRef(env->FindClass("java/util/Map$Entry"));
    tlsCtx.cSet     = (jclass) env->NewGlobalRef(env->FindClass("java/util/Set"));
    tlsCtx.midBoolValue   = env->GetMethodID(tlsCtx.cBoolean, "booleanValue", "()Z");
    tlsCtx.midIntValue    = env->GetMethodID(tlsCtx.cInteger, "intValue",     "()I");
    tlsCtx.midLongValue   = env->GetMethodID(tlsCtx.cLong,    "longValue",    "()J");
    tlsCtx.midFloatValue  = env->GetMethodID(tlsCtx.cFloat,   "floatValue",   "()F");
    tlsCtx.midDoubleValue = env->GetMethodID(tlsCtx.cDouble,  "doubleValue",  "()D");
    jclass cObject = env->FindClass("java/lang/Object");
    tlsCtx.midToString = env->GetMethodID(cObject, "toString", "()Ljava/lang/String;");
    env->DeleteLocalRef(cObject);
    tlsCtx.midMapEntrySet = env->GetMethodID(tlsCtx.cMap, "entrySet", "()Ljava/util/Set;");
    tlsCtx.midSetIterator = env->GetMethodID(tlsCtx.cSet, "iterator", "()Ljava/util/Iterator;");
    tlsCtx.midItHasNext   = env->GetMethodID(tlsCtx.cIterator, "hasNext", "()Z");
    tlsCtx.midItNext      = env->GetMethodID(tlsCtx.cIterator, "next",    "()Ljava/lang/Object;");
    tlsCtx.midEntryKey    = env->GetMethodID(tlsCtx.cMapEntry, "getKey",  "()Ljava/lang/Object;");
    tlsCtx.midEntryVal    = env->GetMethodID(tlsCtx.cMapEntry, "getValue","()Ljava/lang/Object;");
    tlsCtx.ready = true;
}
} // anon

tTJSVariant jobjectToVariant(JNIEnv *env, jobject obj) {
    if (!obj) return tTJSVariant();
    prepCtx(env);
    auto &c = tlsCtx;

    if (env->IsInstanceOf(obj, c.cBoolean)) {
        return tTJSVariant((tjs_int)(env->CallBooleanMethod(obj, c.midBoolValue) ? 1 : 0));
    }
    if (env->IsInstanceOf(obj, c.cInteger)) {
        return tTJSVariant((tjs_int)env->CallIntMethod(obj, c.midIntValue));
    }
    if (env->IsInstanceOf(obj, c.cLong)) {
        return tTJSVariant((tTVInteger)env->CallLongMethod(obj, c.midLongValue));
    }
    if (env->IsInstanceOf(obj, c.cFloat)) {
        return tTJSVariant((tTVReal)env->CallFloatMethod(obj, c.midFloatValue));
    }
    if (env->IsInstanceOf(obj, c.cDouble)) {
        return tTJSVariant((tTVReal)env->CallDoubleMethod(obj, c.midDoubleValue));
    }
    if (env->IsInstanceOf(obj, c.cString)) {
        return tTJSVariant(jStringToTtstr(env, (jstring)obj));
    }

    // byte[] 判定: GetObjectClass + 配列クラスチェック
    {
        jclass cls = env->GetObjectClass(obj);
        // class name = "[B"
        jclass classCls = env->FindClass("java/lang/Class");
        jmethodID getName = env->GetMethodID(classCls, "getName", "()Ljava/lang/String;");
        jstring jname = (jstring) env->CallObjectMethod(cls, getName);
        const char *name = env->GetStringUTFChars(jname, nullptr);
        bool isByteArr = (name && strcmp(name, "[B") == 0);
        bool isObjArr  = (name && name[0] == '[' && name[1] == 'L');
        env->ReleaseStringUTFChars(jname, name);
        env->DeleteLocalRef(jname);
        env->DeleteLocalRef(classCls);
        env->DeleteLocalRef(cls);

        if (isByteArr) {
            jbyteArray arr = (jbyteArray)obj;
            jsize len = env->GetArrayLength(arr);
            std::vector<tjs_uint8> buf(len);
            if (len > 0) env->GetByteArrayRegion(arr, 0, len, reinterpret_cast<jbyte*>(buf.data()));
            tTJSVariantOctet *oct = TJSAllocVariantOctet(buf.data(), (tjs_uint)len);
            tTJSVariant ret;
            ret = oct;
            if (oct) oct->Release();
            return ret;
        }
        if (isObjArr) {
            jobjectArray arr = (jobjectArray)obj;
            jsize len = env->GetArrayLength(arr);
            iTJSDispatch2 *dsp = TJSCreateArrayObject();
            ncbPropAccessor acc(dsp);
            for (jsize i = 0; i < len; ++i) {
                jobject e = env->GetObjectArrayElement(arr, i);
                tTJSVariant v = jobjectToVariant(env, e);
                acc.SetValue((tjs_int)i, v);
                if (e) env->DeleteLocalRef(e);
            }
            tTJSVariant ret(dsp, dsp);
            dsp->Release();
            return ret;
        }
    }

    if (env->IsInstanceOf(obj, c.cMap)) {
        iTJSDispatch2 *dsp = TJSCreateDictionaryObject();
        ncbPropAccessor acc(dsp);
        jobject entrySet = env->CallObjectMethod(obj, c.midMapEntrySet);
        if (entrySet) {
            jobject it = env->CallObjectMethod(entrySet, c.midSetIterator);
            if (it) {
                while (env->CallBooleanMethod(it, c.midItHasNext)) {
                    jobject entry = env->CallObjectMethod(it, c.midItNext);
                    if (!entry) continue;
                    jobject key = env->CallObjectMethod(entry, c.midEntryKey);
                    jobject val = env->CallObjectMethod(entry, c.midEntryVal);
                    if (key && env->IsInstanceOf(key, c.cString)) {
                        ttstr keyStr = jStringToTtstr(env, (jstring)key);
                        tTJSVariant v = jobjectToVariant(env, val);
                        acc.SetValue(keyStr.c_str(), v);
                    }
                    if (key) env->DeleteLocalRef(key);
                    if (val) env->DeleteLocalRef(val);
                    env->DeleteLocalRef(entry);
                }
                env->DeleteLocalRef(it);
            }
            env->DeleteLocalRef(entrySet);
        }
        tTJSVariant ret(dsp, dsp);
        dsp->Release();
        return ret;
    }

    // フォールバック: toString() で文字列化
    jstring js = (jstring) env->CallObjectMethod(obj, c.midToString);
    if (js) {
        ttstr s = jStringToTtstr(env, js);
        env->DeleteLocalRef(js);
        return tTJSVariant(s);
    }
    return tTJSVariant();
}

jobject variantToJObject(JNIEnv *env, const tTJSVariant &v) {
    switch (v.Type()) {
    case tvtVoid:
        return nullptr;
    case tvtInteger: {
        jclass c = env->FindClass("java/lang/Long");
        jmethodID m = env->GetStaticMethodID(c, "valueOf", "(J)Ljava/lang/Long;");
        jobject r = env->CallStaticObjectMethod(c, m, (jlong)(tTVInteger)v);
        env->DeleteLocalRef(c);
        return r;
    }
    case tvtReal: {
        jclass c = env->FindClass("java/lang/Double");
        jmethodID m = env->GetStaticMethodID(c, "valueOf", "(D)Ljava/lang/Double;");
        jobject r = env->CallStaticObjectMethod(c, m, (jdouble)(tTVReal)v);
        env->DeleteLocalRef(c);
        return r;
    }
    case tvtString: {
        ttstr s = v.AsStringNoAddRef();
        return ttstrToJString(env, s);
    }
    case tvtOctet: {
        tTJSVariantOctet *o = v.AsOctetNoAddRef();
        if (!o) return nullptr;
        const tjs_uint8 *p = o->GetData();
        tjs_uint len = o->GetLength();
        jbyteArray arr = env->NewByteArray((jsize)len);
        if (arr && len > 0) env->SetByteArrayRegion(arr, 0, len, reinterpret_cast<const jbyte*>(p));
        return arr;
    }
    case tvtObject:
        // 最低限の対応: 再帰的な Map/Array 変換は今のところ不要 (write系のパラメータは
        // String / byte[] / 整数のみ受ける設計)。
        return nullptr;
    default:
        return nullptr;
    }
}

// -------------------- callback queue --------------------

struct PendingCallback {
    tjs_int64 id;
    bool      isError;
    tTJSVariant value;     // 成功時
    ttstr        message;  // エラー時
};

static std::mutex                     gCbMutex;
static std::atomic<tjs_int64>         gNextId{1};
static std::unordered_map<tjs_int64, tTJSVariantClosure> gRegistered;
static std::deque<PendingCallback>    gPending;

tjs_int64 registerCallback(const tTJSVariantClosure &closure) {
    if (closure.Object == nullptr) return 0;
    tTJSVariantClosure local(closure.Object, closure.ObjThis);
    local.AddRef();
    tjs_int64 id = gNextId.fetch_add(1);
    {
        std::lock_guard<std::mutex> g(gCbMutex);
        gRegistered.emplace(id, local);
    }
    return id;
}

void postResult(tjs_int64 id, const tTJSVariant &v) {
    if (id == 0) return;
    std::lock_guard<std::mutex> g(gCbMutex);
    PendingCallback p;
    p.id = id;
    p.isError = false;
    p.value = v;
    gPending.emplace_back(std::move(p));
}

void postError(tjs_int64 id, const ttstr &msg) {
    if (id == 0) return;
    std::lock_guard<std::mutex> g(gCbMutex);
    PendingCallback p;
    p.id = id;
    p.isError = true;
    p.message = msg;
    gPending.emplace_back(std::move(p));
}

void drainCallbacks() {
    std::deque<PendingCallback> local;
    {
        std::lock_guard<std::mutex> g(gCbMutex);
        local.swap(gPending);
    }
    for (auto &p : local) {
        tTJSVariantClosure closure;
        bool found = false;
        {
            std::lock_guard<std::mutex> g(gCbMutex);
            auto it = gRegistered.find(p.id);
            if (it != gRegistered.end()) {
                closure = it->second;
                gRegistered.erase(it);
                found = true;
            }
        }
        if (!found || !closure.Object) continue;
        // closure(success_or_null_value, error_msg_or_void) で呼び出す
        tTJSVariant args[2];
        if (p.isError) {
            args[0].Clear();
            args[1] = tTJSVariant(p.message);
        } else {
            args[0] = p.value;
            args[1].Clear();
        }
        tTJSVariant *pargs[2] = { &args[0], &args[1] };
        tTJSVariant ret;
        try {
            closure.FuncCall(0, nullptr, nullptr, &ret, 2, pargs, nullptr);
        } catch (...) {
            // TJS 例外は上に投げると continuous hook 全体が止まるので握り潰しのみ
            LOGE("callback threw exception (id=%lld)", (long long)p.id);
        }
        closure.Release();
    }
}

void shutdownCallbacks() {
    std::lock_guard<std::mutex> g(gCbMutex);
    for (auto &kv : gRegistered) kv.second.Release();
    gRegistered.clear();
    gPending.clear();
}

} // namespace playgames

// -------------------- JNI entry points (called from Java) --------------------

extern "C" {

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void* /*reserved*/) {
    playgames::setJavaVm(vm);
    LOGI("JNI_OnLoad: JavaVM cached");
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL
Java_jp_wamsoft_krkrz_playgames_PlayGamesNative_nativePostResult(
        JNIEnv *env, jclass /*cls*/, jlong callbackId, jobject result) {
    tTJSVariant v = playgames::jobjectToVariant(env, result);
    playgames::postResult((tjs_int64)callbackId, v);
}

JNIEXPORT void JNICALL
Java_jp_wamsoft_krkrz_playgames_PlayGamesNative_nativePostError(
        JNIEnv *env, jclass /*cls*/, jlong callbackId, jstring jmsg) {
    ttstr m = playgames::jStringToTtstr(env, jmsg);
    playgames::postError((tjs_int64)callbackId, m);
}

} // extern "C"
