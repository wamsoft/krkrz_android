package jp.wamsoft.krkrz.license;

/**
 * KrkrZ エンジン内部が保持する権利情報 (license.txt + KrkrZ/TJS バージョン文字列を
 * 組み立てたもの = TJS System.licenseText 相当) を Java から取得するためのブリッジ。
 * libkrkrz.so が System.loadLibrary 済みであることが前提。
 */
public final class LicenseBridge {
    private LicenseBridge() {}

    /**
     * 権利情報の全文を返す。エンジン未初期化時や読み込み失敗時は空文字を返す
     * (例外は throw されない)。
     */
    public static native String getLicenseText();
}
