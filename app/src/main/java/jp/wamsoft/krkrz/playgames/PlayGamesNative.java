package jp.wamsoft.krkrz.playgames;

/**
 * Java -> Native コールバックブリッジ。
 *
 * Java側で非同期処理 (Task) が完了したら、registered な callbackId に対して
 * 結果またはエラーをポストする。Native 側は libkrkrz.so に実装され、結果を
 * スレッドセーフキューに積み、TJS 側のコンティニュアスイベントで取り出して
 * 元のコールバック関数を呼び出す。
 *
 * 結果オブジェクトは Java の標準型 (Boolean / Integer / Long / Double /
 * String / Object[] / java.util.Map) で渡す。Native側で tTJSVariant に
 * 変換される。
 */
public final class PlayGamesNative {
    private PlayGamesNative() {}

    public static native void nativePostResult(long callbackId, Object result);
    public static native void nativePostError(long callbackId, String errorMessage);

    /**
     * Java 側 helper: Map.of 系を最低 API 24 で使うため自前で生成。
     */
    public static java.util.HashMap<String, Object> dict() {
        return new java.util.HashMap<>();
    }
}
