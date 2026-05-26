// VirtualPad JNI bridge - Java の VirtualPadBridge から native 関数を呼ぶための実装。
// JNIEXPORT で書かれているので krkrz_plugin マクロの --whole-archive リンカフラグで
// libkrkrz.so から露出される (playgames プラグインと同じ流儀)。

#include <jni.h>
#include "VirtualPad.h"

extern "C" {

JNIEXPORT void JNICALL
Java_jp_wamsoft_krkrz_virtualpad_VirtualPadBridge_setEnabled(
        JNIEnv * /*env*/, jclass /*cls*/, jboolean enabled) {
    virtualpad::SetEnabled(enabled == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_jp_wamsoft_krkrz_virtualpad_VirtualPadBridge_setOpacity(
        JNIEnv * /*env*/, jclass /*cls*/, jfloat opacity) {
    virtualpad::SetOpacity((float)opacity);
}

JNIEXPORT void JNICALL
Java_jp_wamsoft_krkrz_virtualpad_VirtualPadBridge_setPreset(
        JNIEnv * /*env*/, jclass /*cls*/, jint preset) {
    virtualpad::SetPreset((int)preset);
}

} // extern "C"
