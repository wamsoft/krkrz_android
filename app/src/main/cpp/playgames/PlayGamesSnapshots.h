#pragma once
#include "tp_stub.h"

namespace playgames {

tjs_error CallListSnapshots(const tTJSVariantClosure &cb, bool forceReload);
tjs_error CallReadSnapshot(const tTJSVariantClosure &cb, const ttstr &uniqueName);
// data は string または octet
tjs_error CallWriteSnapshot(const tTJSVariantClosure &cb,
                             const ttstr &uniqueName,
                             const tTJSVariant &data,
                             const ttstr &description,
                             tjs_int64 playedTimeMs,
                             tjs_int64 progressValue);
tjs_error CallDeleteSnapshot(const tTJSVariantClosure &cb, const ttstr &uniqueName);
tjs_error CallShowSnapshotsUI(const tTJSVariantClosure &cb,
                                const ttstr &title,
                                bool allowAddBtn,
                                bool allowDeleteBtn,
                                tjs_int maxSnapshots);

} // namespace playgames
