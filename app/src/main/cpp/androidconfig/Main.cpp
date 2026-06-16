// androidconfig プラグイン本体 - continuous event hook の登録のみ。
//
// playgames プラグインと同じ流儀:
//   - tTVPContinuousEventCallbackIntf を毎フレーム駆動し、Java からキューに積まれた
//     設定リクエストを TJS メインスレッド上で drain する。
//   - NCB_REGISTER_CALLBACK(PreRegist/PostRegist) で hook の登録・解除を行う。
//
// 実際の処理ロジック・JNI エントリポイントは ConfigBridge.cpp 側。

#include "tp_stub.h"
#include "ncbind.hpp"
#include "ConfigBridge.h"

#include <android/log.h>
#define LOG_TAG "androidconfig"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

namespace {

class ConfigEventDrain : public tTVPContinuousEventCallbackIntf {
public:
    void TJS_INTF_METHOD OnContinuousCallback(tjs_uint64 /*tick*/) override {
        androidconfig::drainRequests();
    }
};
ConfigEventDrain *gDrain = nullptr;

void registerPlugin() {
    if (!gDrain) {
        gDrain = new ConfigEventDrain();
        TVPAddContinuousEventHook(gDrain);
        LOGI("plugin registered: continuous event hook installed");
    }
}

void unregisterPlugin() {
    if (gDrain) {
        TVPRemoveContinuousEventHook(gDrain);
        delete gDrain;
        gDrain = nullptr;
    }
    androidconfig::shutdown();
}

} // anonymous

NCB_REGISTER_CALLBACK(PreRegist,  registerPlugin,   0, registerAndroidConfig_0);
NCB_REGISTER_CALLBACK(PostRegist, 0, unregisterPlugin, unregisterAndroidConfig_0);
