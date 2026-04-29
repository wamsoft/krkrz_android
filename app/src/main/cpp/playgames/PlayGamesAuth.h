#pragma once
#include "tp_stub.h"

namespace playgames {

// Java 側 PlayGamesAuth の薄いラッパー。
// callbackClosure は (result, error) を受ける関数。
tjs_error CallIsAuthenticated(const tTJSVariantClosure &cb);
tjs_error CallSignIn(const tTJSVariantClosure &cb);
tjs_error CallCurrentPlayer(const tTJSVariantClosure &cb);

} // namespace playgames
