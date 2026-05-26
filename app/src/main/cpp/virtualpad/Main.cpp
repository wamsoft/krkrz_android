// VirtualPad プラグイン: TJS バインディング + プラグイン登録フック
//
// PreRegist で SDL 仮想ジョイスティックの attach + post-render コールバック登録、
// PostRegist で detach + コールバック解除。

#include "tp_stub.h"
#include "ncbind.hpp"
#include "VirtualPad.h"

namespace {

class VirtualPad {
public:
    static tjs_error Factory(VirtualPad** /*obj*/, tjs_int /*np*/,
                             tTJSVariant** /*p*/, iTJSDispatch2* /*objthis*/) {
        TVPThrowExceptionMessage(TJSGetMessageMapMessage(TJS_W("TVPCannotCreateInstance")).c_str());
        return TJS_E_FAIL;
    }

    static void registerPlugin() {
        virtualpad::Init();
    }

    static void unregisterPlugin() {
        virtualpad::Shutdown();
    }

    // --- TJS から呼べる API ---

    static void setEnabled(bool enabled) {
        virtualpad::SetEnabled(enabled);
    }

    static void setOpacity(double opacity) {
        virtualpad::SetOpacity((float)opacity);
    }

    static void setPreset(tjs_int preset) {
        virtualpad::SetPreset(preset);
    }
};

} // anonymous

NCB_REGISTER_CLASS(VirtualPad) {
    Factory(&Class::Factory);
    NCB_METHOD(setEnabled);
    NCB_METHOD(setOpacity);
    NCB_METHOD(setPreset);
}

NCB_REGISTER_CALLBACK(PreRegist,  VirtualPad::registerPlugin,   0, registerVirtualPad_0);
NCB_REGISTER_CALLBACK(PostRegist, 0, VirtualPad::unregisterPlugin, unregisterVirtualPad_0);
