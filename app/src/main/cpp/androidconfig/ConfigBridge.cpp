#include "ConfigBridge.h"

#include "tp_stub.h"
#include "ncbind.hpp"   // ncbPropAccessor / TJSCreateDictionaryObject ラッパ

#include <android/log.h>
#include <mutex>
#include <deque>
#include <string>
#include <vector>

#define LOG_TAG "androidconfig"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)

namespace androidconfig {

// ============================================================
// JavaVM / JNIEnv
// ============================================================

static JavaVM *gJvm = nullptr;

static void cacheJavaVm(JNIEnv *env) {
    if (gJvm) return;
    if (env) env->GetJavaVM(&gJvm);
}

namespace {
// drain は TJS スレッドで動く。そのスレッドは通常 JNI に未アタッチなので
// AttachCurrentThread して env を確保する。
struct ScopedEnv {
    JNIEnv *env_ = nullptr;
    bool attached_ = false;
    ScopedEnv() {
        if (!gJvm) return;
        int st = gJvm->GetEnv(reinterpret_cast<void**>(&env_), JNI_VERSION_1_6);
        if (st == JNI_OK) {
            attached_ = false;
        } else if (st == JNI_EDETACHED) {
            if (gJvm->AttachCurrentThread(&env_, nullptr) == JNI_OK) attached_ = true;
            else env_ = nullptr;
        } else {
            env_ = nullptr;
        }
    }
    ~ScopedEnv() { if (attached_ && gJvm) gJvm->DetachCurrentThread(); }
    JNIEnv *get() const { return env_; }
};
} // anon

// ============================================================
// 文字列変換
// ============================================================

static jstring ttstrToJString(JNIEnv *env, const ttstr &s) {
    if (s.IsEmpty()) return env->NewStringUTF("");
    int maxlen = (int)s.length() * 6 + 1;
    std::vector<char> buf(maxlen);
    int len = TVPWideCharToUtf8String(s.c_str(), buf.data());
    if (len < 0) len = 0;
    buf[len] = '\0';
    return env->NewStringUTF(buf.data());
}

static ttstr jStringToTtstr(JNIEnv *env, jstring js) {
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

static ttstr utf8ToTtstr(const std::string &u) {
    if (u.empty()) return ttstr();
    std::vector<tjs_char> buf(u.size() * 2 + 1);
    tjs_uint w = TVPUtf8ToWideCharString(u.c_str(), buf.data());
    return ttstr(buf.data(), w);
}

// ============================================================
// TJS グローバル関数呼び出し (存在すれば呼ぶ)
// ============================================================

// global.<name>(param...) を呼ぶ。存在し呼べたら true。result に戻り値。
static bool callGlobal(const tjs_char *name, tjs_int nparam,
                       tTJSVariant **param, tTJSVariant *result) {
    iTJSDispatch2 *global = TVPGetScriptDispatch(); // AddRef 済み
    if (!global) return false;
    bool ok = false;
    try {
        tTJSVariant func;
        tjs_error e = global->PropGet(0, name, nullptr, &func, global);
        if (TJS_SUCCEEDED(e) && func.Type() == tvtObject) {
            tTJSVariantClosure clo = func.AsObjectClosureNoAddRef();
            if (clo.Object) {
                tjs_error fe = clo.FuncCall(0, nullptr, nullptr, result, nparam, param, global);
                ok = TJS_SUCCEEDED(fe);
            }
        }
    } catch (...) {
        LOGE("callGlobal threw (a TJS global threw an exception)");
        ok = false;
    }
    global->Release();
    return ok;
}

// ============================================================
// Java HashMap ヘルパ
// ============================================================

namespace {
struct JMap {
    jclass cHashMap = nullptr;
    jmethodID ctor = nullptr;
    jmethodID put = nullptr;
    jclass cLong = nullptr;
    jmethodID longVal = nullptr;
    jclass cDouble = nullptr;
    jmethodID doubleVal = nullptr;
    jclass cInteger = nullptr;
    jmethodID intVal = nullptr;
    bool ready = false;
    void init(JNIEnv *env) {
        if (ready) return;
        cHashMap = (jclass) env->NewGlobalRef(env->FindClass("java/util/HashMap"));
        ctor = env->GetMethodID(cHashMap, "<init>", "()V");
        put  = env->GetMethodID(cHashMap, "put",
                "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
        cLong = (jclass) env->NewGlobalRef(env->FindClass("java/lang/Long"));
        longVal = env->GetStaticMethodID(cLong, "valueOf", "(J)Ljava/lang/Long;");
        cDouble = (jclass) env->NewGlobalRef(env->FindClass("java/lang/Double"));
        doubleVal = env->GetStaticMethodID(cDouble, "valueOf", "(D)Ljava/lang/Double;");
        cInteger = (jclass) env->NewGlobalRef(env->FindClass("java/lang/Integer"));
        intVal = env->GetStaticMethodID(cInteger, "valueOf", "(I)Ljava/lang/Integer;");
        ready = true;
    }
};
static JMap gJMap;
} // anon

static jobject newHashMap(JNIEnv *env) {
    gJMap.init(env);
    return env->NewObject(gJMap.cHashMap, gJMap.ctor);
}

static void mapPutStr(JNIEnv *env, jobject map, const char *key, jobject value) {
    jstring k = env->NewStringUTF(key);
    env->CallObjectMethod(map, gJMap.put, k, value);
    env->DeleteLocalRef(k);
    if (value) env->DeleteLocalRef(value);
}

static void mapPutInt(JNIEnv *env, jobject map, const char *key, jint v) {
    jobject boxed = env->CallStaticObjectMethod(gJMap.cInteger, gJMap.intVal, v);
    mapPutStr(env, map, key, boxed);
}

// TJS の値 (scalar) を Java の Long/Double/String に変換
static jobject variantScalarToJObject(JNIEnv *env, const tTJSVariant &v) {
    gJMap.init(env);
    switch (v.Type()) {
    case tvtInteger:
        return env->CallStaticObjectMethod(gJMap.cLong, gJMap.longVal,
                                           (jlong)(tTVInteger)v);
    case tvtReal:
        return env->CallStaticObjectMethod(gJMap.cDouble, gJMap.doubleVal,
                                           (jdouble)(tTVReal)v);
    case tvtString: {
        ttstr s = v.AsStringNoAddRef();
        return ttstrToJString(env, s);
    }
    case tvtVoid:
    default:
        // Dictionary/Array/Octet 等はフラット契約では来ない想定。無視 (null)。
        return nullptr;
    }
}

// ============================================================
// 辞書 (iTJSDispatch2) -> Java HashMap (EnumMembers 列挙)
//   json プラグインの DictMemberDispCaller と同じイディオム。
// ============================================================

namespace {
class DictToMapCaller : public tTJSDispatch {
    JNIEnv *env_;
    jobject map_;
public:
    DictToMapCaller(JNIEnv *env, jobject map) : env_(env), map_(map) {}
    virtual tjs_error TJS_INTF_METHOD FuncCall(
            tjs_uint32 /*flag*/, const tjs_char * /*membername*/, tjs_uint32 * /*hint*/,
            tTJSVariant *result, tjs_int numparams, tTJSVariant **param,
            iTJSDispatch2 * /*objthis*/) override {
        // param[0]=name, param[1]=flags, param[2]=value
        if (numparams >= 3 && param[0] && param[2]) {
            tTVInteger flag = (numparams >= 2 && param[1]) ? param[1]->AsInteger() : 0;
            if (!(flag & TJS_HIDDENMEMBER)) {
                ttstr name = param[0]->GetString() ? ttstr(param[0]->GetString()) : ttstr();
                jstring jkey = ttstrToJString(env_, name);
                jobject jval = variantScalarToJObject(env_, *param[2]);
                env_->CallObjectMethod(map_, gJMap.put, jkey, jval);
                env_->DeleteLocalRef(jkey);
                if (jval) env_->DeleteLocalRef(jval);
            }
        }
        if (result) *result = true;
        return TJS_S_OK;
    }
};
} // anon

static jobject dictToHashMap(JNIEnv *env, iTJSDispatch2 *dict) {
    jobject map = newHashMap(env);
    if (!dict) return map;
    DictToMapCaller *caller = new DictToMapCaller(env, map);
    tTJSVariantClosure closure(caller);
    try {
        dict->EnumMembers(TJS_IGNOREPROP, &closure, dict);
    } catch (...) {
        LOGE("EnumMembers threw");
    }
    caller->Release();
    return map;
}

// ============================================================
// Java Map -> TJS Dictionary (apply 用)
// ============================================================

namespace {
struct ConvCtx {
    jclass cBoolean=nullptr, cInteger=nullptr, cLong=nullptr,
           cFloat=nullptr, cDouble=nullptr, cString=nullptr,
           cMap=nullptr, cSet=nullptr, cIterator=nullptr, cMapEntry=nullptr;
    jmethodID midBool=nullptr, midInt=nullptr, midLong=nullptr,
              midFloat=nullptr, midDouble=nullptr,
              midEntrySet=nullptr, midSetIt=nullptr,
              midHasNext=nullptr, midNext=nullptr, midKey=nullptr, midVal=nullptr;
    bool ready=false;
    void init(JNIEnv *env) {
        if (ready) return;
        auto G=[&](const char*n){return (jclass)env->NewGlobalRef(env->FindClass(n));};
        cBoolean=G("java/lang/Boolean"); cInteger=G("java/lang/Integer");
        cLong=G("java/lang/Long"); cFloat=G("java/lang/Float");
        cDouble=G("java/lang/Double"); cString=G("java/lang/String");
        cMap=G("java/util/Map"); cSet=G("java/util/Set");
        cIterator=G("java/util/Iterator"); cMapEntry=G("java/util/Map$Entry");
        midBool  =env->GetMethodID(cBoolean,"booleanValue","()Z");
        midInt   =env->GetMethodID(cInteger,"intValue","()I");
        midLong  =env->GetMethodID(cLong,"longValue","()J");
        midFloat =env->GetMethodID(cFloat,"floatValue","()F");
        midDouble=env->GetMethodID(cDouble,"doubleValue","()D");
        midEntrySet=env->GetMethodID(cMap,"entrySet","()Ljava/util/Set;");
        midSetIt =env->GetMethodID(cSet,"iterator","()Ljava/util/Iterator;");
        midHasNext=env->GetMethodID(cIterator,"hasNext","()Z");
        midNext  =env->GetMethodID(cIterator,"next","()Ljava/lang/Object;");
        midKey   =env->GetMethodID(cMapEntry,"getKey","()Ljava/lang/Object;");
        midVal   =env->GetMethodID(cMapEntry,"getValue","()Ljava/lang/Object;");
        ready=true;
    }
};
static ConvCtx gConv;
} // anon

static tTJSVariant jobjectToVariant(JNIEnv *env, jobject obj); // fwd

static iTJSDispatch2* jmapToDict(JNIEnv *env, jobject mapObj) {
    gConv.init(env);
    iTJSDispatch2 *dsp = TJSCreateDictionaryObject();
    ncbPropAccessor acc(dsp);
    jobject entrySet = env->CallObjectMethod(mapObj, gConv.midEntrySet);
    if (entrySet) {
        jobject it = env->CallObjectMethod(entrySet, gConv.midSetIt);
        if (it) {
            while (env->CallBooleanMethod(it, gConv.midHasNext)) {
                jobject entry = env->CallObjectMethod(it, gConv.midNext);
                if (!entry) continue;
                jobject key = env->CallObjectMethod(entry, gConv.midKey);
                jobject val = env->CallObjectMethod(entry, gConv.midVal);
                if (key && env->IsInstanceOf(key, gConv.cString)) {
                    ttstr k = jStringToTtstr(env, (jstring)key);
                    tTJSVariant v = jobjectToVariant(env, val);
                    acc.SetValue(k.c_str(), v);
                }
                if (key) env->DeleteLocalRef(key);
                if (val) env->DeleteLocalRef(val);
                env->DeleteLocalRef(entry);
            }
            env->DeleteLocalRef(it);
        }
        env->DeleteLocalRef(entrySet);
    }
    return dsp;
}

static tTJSVariant jobjectToVariant(JNIEnv *env, jobject obj) {
    if (!obj) return tTJSVariant();
    gConv.init(env);
    auto &c = gConv;
    if (env->IsInstanceOf(obj, c.cBoolean))
        return tTJSVariant((tjs_int)(env->CallBooleanMethod(obj, c.midBool) ? 1 : 0));
    if (env->IsInstanceOf(obj, c.cInteger))
        return tTJSVariant((tjs_int)env->CallIntMethod(obj, c.midInt));
    if (env->IsInstanceOf(obj, c.cLong))
        return tTJSVariant((tTVInteger)env->CallLongMethod(obj, c.midLong));
    if (env->IsInstanceOf(obj, c.cFloat))
        return tTJSVariant((tTVReal)env->CallFloatMethod(obj, c.midFloat));
    if (env->IsInstanceOf(obj, c.cDouble))
        return tTJSVariant((tTVReal)env->CallDoubleMethod(obj, c.midDouble));
    if (env->IsInstanceOf(obj, c.cString))
        return tTJSVariant(jStringToTtstr(env, (jstring)obj));
    if (env->IsInstanceOf(obj, c.cMap)) {
        iTJSDispatch2 *d = jmapToDict(env, obj);
        tTJSVariant ret(d, d);
        d->Release();
        return ret;
    }
    return tTJSVariant();
}

// ============================================================
// リクエストキュー
// ============================================================

enum class Op { Fetch, Apply, SetVolume };

struct Request {
    Op op;
    jobject applyMap = nullptr; // Apply: Java Map の global ref
    std::string volKey;         // SetVolume: utf8 key
    int volValue = 0;           // SetVolume
};

static std::mutex gQueueMutex;
static std::deque<Request> gQueue;

static void enqueue(Request r) {
    std::lock_guard<std::mutex> g(gQueueMutex);
    gQueue.emplace_back(std::move(r));
}

// 既知の基本音量キー
static const char *kBaseVolumeKeys[] = {
    "master", "bgm", "se", "systemSe", "voice", "systemVoice"
};

// KrkrzAndroidGetVolume(key) -> int (失敗時 -1)
static int getVolume(const ttstr &key) {
    tTJSVariant keyv(key);
    tTJSVariant *param[1] = { &keyv };
    tTJSVariant result;
    if (!callGlobal(TJS_W("KrkrzAndroidGetVolume"), 1, param, &result)) return -1;
    if (result.Type() == tvtVoid) return -1;
    return (int)(tTVInteger)result;
}

// 設定取得 + 音量取得 -> Java の KrkrzConfig.onConfigReady(config, volumes) へ
static void doFetch(JNIEnv *env) {
    jclass cls = env->FindClass("jp/wamsoft/krkrz/config/KrkrzConfig");
    if (!cls) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        LOGE("KrkrzConfig class not found");
        return;
    }
    jmethodID onReady = env->GetStaticMethodID(cls, "onConfigReady",
            "(Ljava/util/Map;Ljava/util/Map;)V");
    if (!onReady) { if (env->ExceptionCheck()) env->ExceptionClear(); return; }

    // KrkrzAndroidGetConfig()
    LOGI("doFetch: calling KrkrzAndroidGetConfig");
    tTJSVariant cfgResult;
    bool haveCfg = callGlobal(TJS_W("KrkrzAndroidGetConfig"), 0, nullptr, &cfgResult);
    LOGI("doFetch: GetConfig haveCfg=%d resultType=%d", (int)haveCfg, (int)cfgResult.Type());
    if (!haveCfg || cfgResult.Type() != tvtObject) {
        LOGI("KrkrzAndroidGetConfig unavailable; reporting null config");
        env->CallStaticVoidMethod(cls, onReady, (jobject)nullptr, (jobject)nullptr);
        env->DeleteLocalRef(cls);
        return;
    }

    iTJSDispatch2 *dict = cfgResult.AsObjectNoAddRef();
    jobject configMap = dictToHashMap(env, dict);

    // 音量: 基本キー + charVoiceNames から派生したキャラ別キー
    jobject volMap = newHashMap(env);
    for (const char *k : kBaseVolumeKeys) {
        int v = getVolume(utf8ToTtstr(k));
        if (v >= 0) mapPutInt(env, volMap, k, v);
    }
    // charVoiceNames (改行区切り文字列) を読む
    {
        ttstr names;
        try {
            tTJSVariant nv;
            if (TJS_SUCCEEDED(dict->PropGet(0, TJS_W("charVoiceNames"), nullptr, &nv, dict))
                && nv.Type() == tvtString) {
                names = nv.AsStringNoAddRef();
            }
        } catch (...) {}
        if (!names.IsEmpty()) {
            // tjs_char 単位で走査し、改行で区切ってキャラ名を取り出す。
            auto flush = [&](const ttstr &name) {
                if (name.IsEmpty()) return;
                ttstr key = ttstr(TJS_W("charVoice:")) + name;
                int v = getVolume(key);
                if (v >= 0) {
                    // utf8 key を作って put
                    int maxlen = (int)key.length() * 6 + 1;
                    std::vector<char> buf(maxlen);
                    int len = TVPWideCharToUtf8String(key.c_str(), buf.data());
                    if (len < 0) len = 0; buf[len] = '\0';
                    mapPutInt(env, volMap, buf.data(), v);
                }
            };
            ttstr acc2;
            for (const tjs_char *q = names.c_str(); *q; ++q) {
                if (*q == TJS_W('\n') || *q == TJS_W('\r')) {
                    flush(acc2); acc2 = ttstr();
                } else {
                    tjs_char one[2] = { *q, 0 };
                    acc2 += ttstr(one);
                }
            }
            flush(acc2);
        }
    }

    env->CallStaticVoidMethod(cls, onReady, configMap, volMap);
    env->DeleteLocalRef(configMap);
    env->DeleteLocalRef(volMap);
    env->DeleteLocalRef(cls);
}

static void doApply(JNIEnv *env, jobject mapGlobalRef) {
    if (!mapGlobalRef) return;
    iTJSDispatch2 *dict = jmapToDict(env, mapGlobalRef);
    tTJSVariant dv(dict, dict);
    dict->Release();
    tTJSVariant *param[1] = { &dv };
    tTJSVariant result;
    if (callGlobal(TJS_W("KrkrzAndroidSetConfig"), 1, param, &result)) {
        LOGI("doApply: KrkrzAndroidSetConfig called");
    } else {
        LOGI("KrkrzAndroidSetConfig unavailable");
    }
}

static void doSetVolume(const std::string &key, int value) {
    tTJSVariant keyv(utf8ToTtstr(key));
    tTJSVariant valv((tjs_int)value);
    tTJSVariant *param[2] = { &keyv, &valv };
    tTJSVariant result;
    if (callGlobal(TJS_W("KrkrzAndroidSetVolume"), 2, param, &result)) {
        LOGI("doSetVolume: KrkrzAndroidSetVolume(%s, %d) called", key.c_str(), value);
    } else {
        LOGI("KrkrzAndroidSetVolume unavailable");
    }
}

// ============================================================
// drain (TJS スレッド)
// ============================================================

void drainRequests() {
    std::deque<Request> local;
    {
        std::lock_guard<std::mutex> g(gQueueMutex);
        if (gQueue.empty()) return;
        local.swap(gQueue);
    }
    LOGI("drainRequests: processing %d op(s)", (int)local.size());
    ScopedEnv se;
    JNIEnv *env = se.get();
    if (!env) LOGE("drainRequests: JNIEnv unavailable (gJvm=%p)", (void*)gJvm);
    for (auto &r : local) {
        try {
            switch (r.op) {
            case Op::Fetch:      if (env) doFetch(env); break;
            case Op::Apply:      if (env) doApply(env, r.applyMap); break;
            case Op::SetVolume:  doSetVolume(r.volKey, r.volValue); break;
            }
        } catch (...) {
            LOGE("drain op threw");
        }
        if (r.applyMap && env) env->DeleteGlobalRef(r.applyMap);
    }
}

void shutdown() {
    std::deque<Request> local;
    {
        std::lock_guard<std::mutex> g(gQueueMutex);
        local.swap(gQueue);
    }
    if (gJvm) {
        ScopedEnv se;
        JNIEnv *env = se.get();
        if (env) for (auto &r : local) if (r.applyMap) env->DeleteGlobalRef(r.applyMap);
    }
}

} // namespace androidconfig

// ============================================================
// JNI エントリポイント (Java の KrkrzConfig から呼ばれる)
// ============================================================

using namespace androidconfig;

extern "C" {

JNIEXPORT void JNICALL
Java_jp_wamsoft_krkrz_config_KrkrzConfig_nativeRequestConfig(JNIEnv *env, jclass) {
    cacheJavaVm(env);
    LOGI("nativeRequestConfig: enqueue Fetch");
    Request r; r.op = Op::Fetch;
    enqueue(std::move(r));
}

JNIEXPORT void JNICALL
Java_jp_wamsoft_krkrz_config_KrkrzConfig_nativeApplyConfig(JNIEnv *env, jclass, jobject map) {
    cacheJavaVm(env);
    Request r; r.op = Op::Apply;
    r.applyMap = map ? env->NewGlobalRef(map) : nullptr;
    enqueue(std::move(r));
}

JNIEXPORT void JNICALL
Java_jp_wamsoft_krkrz_config_KrkrzConfig_nativeSetVolume(JNIEnv *env, jclass,
                                                         jstring key, jint value) {
    cacheJavaVm(env);
    Request r; r.op = Op::SetVolume; r.volValue = (int)value;
    if (key) {
        const char *u = env->GetStringUTFChars(key, nullptr);
        if (u) { r.volKey = u; env->ReleaseStringUTFChars(key, u); }
    }
    enqueue(std::move(r));
}

} // extern "C"
