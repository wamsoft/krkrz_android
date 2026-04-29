// PlayGames プラグイン本体 - TJS バインディング
//
// Steam プラグインの Main.cpp と構造を揃えている:
//   - 静的シングルトンクラスを Factory 禁止で登録
//   - tTVPContinuousEventCallbackIntf を使って毎フレーム何かを駆動
//     (Steam: SteamAPI_RunCallbacks / PlayGames: コールバックキュー drain)
//   - NCB_REGISTER_CALLBACK(PreRegist/PostRegist) で初期化と解放
//
// 違い:
//   - Steam は同期 API が多いが、Play Games は基本非同期 (Task)。
//     したがって各メソッドは「callback function」を引数に取り、結果を
//     callback(result, errorOrVoid) として返す。
//   - エラー時は引数 result=void, error=ttstr。
//   - 成功時は引数 result=値, error=void。

#include "tp_stub.h"
#include "ncbind.hpp"
#include "JniBridge.h"
#include "PlayGamesAuth.h"
#include "PlayGamesAchievements.h"
#include "PlayGamesSnapshots.h"

namespace {

// closure を引数 0 から取り出すヘルパ
// callback が省略 (numparams<idx+1 || param[idx]==void) ならデフォルト closure
static tTJSVariantClosure pickClosure(tjs_int numparams, tTJSVariant **param, tjs_int idx) {
    if (numparams <= idx) return tTJSVariantClosure(nullptr, nullptr);
    tTJSVariant *v = param[idx];
    if (!v || v->Type() != tvtObject) return tTJSVariantClosure(nullptr, nullptr);
    return v->AsObjectClosureNoAddRef();
}

class PlayGamesEventDrain : public tTVPContinuousEventCallbackIntf {
public:
    void TJS_INTF_METHOD OnContinuousCallback(tjs_uint64 /*tick*/) override {
        playgames::drainCallbacks();
    }
};
PlayGamesEventDrain *gDrain = nullptr;

// ---------------------------------------------------------
// PlayGames 静的クラス (Steam の Steam クラスと同じパターン)
// ---------------------------------------------------------

class PlayGames {
public:
    static tjs_error Factory(PlayGames** /*obj*/, tjs_int /*np*/,
                             tTJSVariant** /*p*/, iTJSDispatch2* /*objthis*/) {
        TVPThrowExceptionMessage(TJSGetMessageMapMessage(TJS_W("TVPCannotCreateInstance")).c_str());
        return TJS_E_FAIL;
    }

    static void registerPlugin() {
        if (!gDrain) {
            gDrain = new PlayGamesEventDrain();
            TVPAddContinuousEventHook(gDrain);
        }
    }

    static void unregisterPlugin() {
        if (gDrain) {
            TVPRemoveContinuousEventHook(gDrain);
            delete gDrain;
            gDrain = nullptr;
        }
        playgames::shutdownCallbacks();
    }

    // ---- Auth ----

    static void isAuthenticated(tTJSVariant cb) {
        playgames::CallIsAuthenticated(cb.AsObjectClosureNoAddRef());
    }
    static void signIn(tTJSVariant cb) {
        playgames::CallSignIn(cb.AsObjectClosureNoAddRef());
    }
    static void getCurrentPlayer(tTJSVariant cb) {
        playgames::CallCurrentPlayer(cb.AsObjectClosureNoAddRef());
    }

    // ---- Achievements ----

    // load(callback, forceReload=false)
    static tjs_error TJS_INTF_METHOD loadAchievements(
            tTJSVariant *result, tjs_int numparams,
            tTJSVariant **param, iTJSDispatch2* /*objthis*/) {
        if (numparams < 1) return TJS_E_BADPARAMCOUNT;
        tTJSVariantClosure cb = pickClosure(numparams, param, 0);
        bool force = (numparams >= 2 && param[1]) ? (bool)(tjs_int)*param[1] : false;
        tjs_error err = playgames::CallLoadAchievements(cb, force);
        if (result) *result = (err == TJS_S_OK);
        return TJS_S_OK;
    }

    // unlock(callback, achievementId)
    // callback が void なら fire-and-forget
    static void unlockAchievement(tTJSVariant cb, ttstr achievementId) {
        playgames::CallUnlockAchievement(cb.AsObjectClosureNoAddRef(), achievementId);
    }

    // increment(callback, achievementId, numSteps)
    static void incrementAchievement(tTJSVariant cb, ttstr achievementId, tjs_int numSteps) {
        playgames::CallIncrementAchievement(cb.AsObjectClosureNoAddRef(), achievementId, numSteps);
    }

    // setSteps(callback, achievementId, numSteps)
    static void setStepsAchievement(tTJSVariant cb, ttstr achievementId, tjs_int numSteps) {
        playgames::CallSetStepsAchievement(cb.AsObjectClosureNoAddRef(), achievementId, numSteps);
    }

    static void showAchievementsUI(tTJSVariant cb) {
        playgames::CallShowAchievementsUI(cb.AsObjectClosureNoAddRef());
    }

    // ---- Snapshots (saved games) ----

    static tjs_error TJS_INTF_METHOD listSnapshots(
            tTJSVariant *result, tjs_int numparams,
            tTJSVariant **param, iTJSDispatch2* /*objthis*/) {
        if (numparams < 1) return TJS_E_BADPARAMCOUNT;
        tTJSVariantClosure cb = pickClosure(numparams, param, 0);
        bool force = (numparams >= 2 && param[1]) ? (bool)(tjs_int)*param[1] : false;
        playgames::CallListSnapshots(cb, force);
        if (result) result->Clear();
        return TJS_S_OK;
    }

    static void readSnapshot(tTJSVariant cb, ttstr uniqueName) {
        playgames::CallReadSnapshot(cb.AsObjectClosureNoAddRef(), uniqueName);
    }

    // writeSnapshot(callback, uniqueName, data, description="", playedTimeMs=-1, progressValue=-1)
    static tjs_error TJS_INTF_METHOD writeSnapshot(
            tTJSVariant *result, tjs_int numparams,
            tTJSVariant **param, iTJSDispatch2* /*objthis*/) {
        if (numparams < 3) return TJS_E_BADPARAMCOUNT;
        tTJSVariantClosure cb = pickClosure(numparams, param, 0);
        ttstr uniqueName = *param[1];
        tTJSVariant data = *param[2];
        ttstr description = (numparams >= 4 && param[3]) ? ttstr(*param[3]) : ttstr();
        tjs_int64 playedTimeMs = (numparams >= 5 && param[4]) ? (tjs_int64)(tTVInteger)*param[4] : -1;
        tjs_int64 progressVal  = (numparams >= 6 && param[5]) ? (tjs_int64)(tTVInteger)*param[5] : -1;
        playgames::CallWriteSnapshot(cb, uniqueName, data, description, playedTimeMs, progressVal);
        if (result) result->Clear();
        return TJS_S_OK;
    }

    static void deleteSnapshot(tTJSVariant cb, ttstr uniqueName) {
        playgames::CallDeleteSnapshot(cb.AsObjectClosureNoAddRef(), uniqueName);
    }

    // showSnapshotsUI(callback, title, allowAdd=true, allowDelete=true, maxSnapshots=0)
    static tjs_error TJS_INTF_METHOD showSnapshotsUI(
            tTJSVariant *result, tjs_int numparams,
            tTJSVariant **param, iTJSDispatch2* /*objthis*/) {
        if (numparams < 2) return TJS_E_BADPARAMCOUNT;
        tTJSVariantClosure cb = pickClosure(numparams, param, 0);
        ttstr title = *param[1];
        bool allowAdd    = (numparams >= 3 && param[2]) ? (bool)(tjs_int)*param[2] : true;
        bool allowDelete = (numparams >= 4 && param[3]) ? (bool)(tjs_int)*param[3] : true;
        tjs_int maxN     = (numparams >= 5 && param[4]) ? (tjs_int)*param[4] : 0;
        playgames::CallShowSnapshotsUI(cb, title, allowAdd, allowDelete, maxN);
        if (result) result->Clear();
        return TJS_S_OK;
    }
};

} // anonymous

NCB_REGISTER_CLASS(PlayGames) {
    Factory(&Class::Factory);

    // --- Auth ---
    NCB_METHOD(isAuthenticated);
    NCB_METHOD(signIn);
    NCB_METHOD(getCurrentPlayer);

    // --- Achievements ---
    RawCallback("loadAchievements",     &Class::loadAchievements,     TJS_STATICMEMBER);
    NCB_METHOD(unlockAchievement);
    NCB_METHOD(incrementAchievement);
    NCB_METHOD(setStepsAchievement);
    NCB_METHOD(showAchievementsUI);

    // --- Snapshots ---
    RawCallback("listSnapshots",        &Class::listSnapshots,        TJS_STATICMEMBER);
    NCB_METHOD(readSnapshot);
    RawCallback("writeSnapshot",        &Class::writeSnapshot,        TJS_STATICMEMBER);
    NCB_METHOD(deleteSnapshot);
    RawCallback("showSnapshotsUI",      &Class::showSnapshotsUI,      TJS_STATICMEMBER);
}

NCB_REGISTER_CALLBACK(PreRegist,  PlayGames::registerPlugin,   0, registerPlayGames_0);
NCB_REGISTER_CALLBACK(PostRegist, 0, PlayGames::unregisterPlugin, unregisterPlayGames_0);
