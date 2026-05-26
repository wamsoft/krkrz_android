// LicenseBridge JNI - KrkrZ エンジンが license.txt から組み立てる権利情報文字列を
// Java へ返す JNI 関数。エンジンが TJS の System.licenseText に出している
// TVPGetLicenseString() を直接呼んで結果を jstring に変換する。

#include <jni.h>
#include <android/log.h>
#include "tp_stub.h"

#include <vector>

#define LIC_LOG(...) __android_log_print(ANDROID_LOG_INFO, "license", __VA_ARGS__)

// エンジン本体 (common/msg/MsgIntf.cpp) で定義
extern ttstr TVPGetLicenseString();

// 静的プラグイン用エントリ関数。エンジン側 (StaticPlugins.cpp 自動生成) が
// tTVPAtStart 経由で起動時に 1 度呼び出す。TJS クラス登録は不要なので no-op。
// NCBIND を使うプラグインは ncbind.cpp がこれを生成するが、この plugin は
// JNI 関数だけ出したいので NCBIND を使っていないため自前で定義する。
extern "C" void krkrz_plugin_license() {
    // no-op
}

extern "C" {

JNIEXPORT jstring JNICALL
Java_jp_wamsoft_krkrz_license_LicenseBridge_getLicenseText(JNIEnv *env, jclass /*cls*/) {
    try {
        ttstr s = TVPGetLicenseString();
        if (s.IsEmpty()) return env->NewStringUTF("");
        // ttstr (UTF-16) -> UTF-8 -> jstring
        int maxlen = (int)s.length() * 6 + 1;
        std::vector<char> buf(maxlen);
        int len = TVPWideCharToUtf8String(s.c_str(), buf.data());
        if (len < 0) len = 0;
        if (len >= maxlen) len = maxlen - 1;
        buf[len] = '\0';
        return env->NewStringUTF(buf.data());
    } catch (const std::exception &e) {
        LIC_LOG("TVPGetLicenseString threw: %s", e.what());
        return env->NewStringUTF("");
    } catch (...) {
        LIC_LOG("TVPGetLicenseString threw (unknown)");
        return env->NewStringUTF("");
    }
}

} // extern "C"
