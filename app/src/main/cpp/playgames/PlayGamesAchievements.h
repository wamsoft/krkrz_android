#pragma once
#include "tp_stub.h"

namespace playgames {

tjs_error CallLoadAchievements(const tTJSVariantClosure &cb, bool forceReload);
tjs_error CallUnlockAchievement(const tTJSVariantClosure &cb, const ttstr &achievementId);
tjs_error CallIncrementAchievement(const tTJSVariantClosure &cb, const ttstr &achievementId, tjs_int numSteps);
tjs_error CallSetStepsAchievement(const tTJSVariantClosure &cb, const ttstr &achievementId, tjs_int numSteps);
tjs_error CallShowAchievementsUI(const tTJSVariantClosure &cb);

} // namespace playgames
