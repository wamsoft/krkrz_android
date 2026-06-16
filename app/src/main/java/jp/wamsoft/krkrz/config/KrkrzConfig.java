package jp.wamsoft.krkrz.config;

import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import java.util.HashMap;
import java.util.Map;

/**
 * 吉里吉里本体 (TJS) 側の設定との橋渡し。
 *
 * <p>native (app/src/main/cpp/androidconfig) を介して以下の TJS グローバル関数を呼ぶ:
 * <ul>
 *   <li>{@code KrkrzAndroidGetConfig()} -> 設定辞書</li>
 *   <li>{@code KrkrzAndroidSetConfig(dict)} -> 設定をまとめて反映</li>
 *   <li>{@code KrkrzAndroidGetVolume(key)} / {@code KrkrzAndroidSetVolume(key, value)} -> 音量 (0-100)</li>
 * </ul>
 *
 * <p>TJS エンジンはマルチスレッド安全ではないため、native 側は実際の TJS 呼び出しを
 * continuous event hook (= TJS メインスレッド) 上で行う。したがって取得は
 * <b>非同期</b>: {@link #requestConfig(Listener)} を呼ぶと、準備でき次第
 * {@link Listener#onConfigReady} がメインスレッドで呼ばれる。
 *
 * <p>libkrkrz.so が System.loadLibrary 済みであること (KrkrzActivity で実施) が前提。
 */
public final class KrkrzConfig {
    private KrkrzConfig() {}

    private static final String TAG = "androidconfig";

    /** 設定準備完了の通知先。すべてメインスレッドで呼ばれる。 */
    public interface Listener {
        /**
         * @param config  設定辞書 (key -> Long/Double/String)。
         *                TJS 側に取得関数が無い場合は {@code null}。
         * @param volumes 音量辞書 (key -> Integer 0-100)。null の場合あり。
         */
        void onConfigReady(Map<String, Object> config, Map<String, Object> volumes);
    }

    private static final Handler MAIN = new Handler(Looper.getMainLooper());
    private static volatile Listener pendingListener;

    // 直近に取得した値のキャッシュ (画面再オープン時の即時表示用)。
    private static volatile Map<String, Object> cachedConfig;
    private static volatile Map<String, Object> cachedVolumes;

    /**
     * TJS 側へ設定を問い合わせる。完了時に listener.onConfigReady がメインスレッドで呼ばれる。
     * native が継続フックで処理するため、エンジン起動直後で取得関数が未定義のときは
     * config=null が返ることがある。
     */
    public static void requestConfig(Listener listener) {
        pendingListener = listener;
        Log.i(TAG, "requestConfig: calling nativeRequestConfig");
        nativeRequestConfig();
    }

    /** 直近のキャッシュ (無ければ null)。画面構築前の暫定表示に使える。 */
    public static Map<String, Object> cachedConfig() { return cachedConfig; }
    public static Map<String, Object> cachedVolumes() { return cachedVolumes; }

    /**
     * 編集後の設定辞書をまとめて TJS 側へ通知する (KrkrzAndroidSetConfig)。
     * 音量は {@link #setVolume} で逐次反映するため、ここには含めない運用を想定。
     */
    public static void applyConfig(Map<String, Object> config) {
        if (config == null) return;
        // native へは java.util.HashMap として渡す (変換は native 側 jmapToDict)。
        nativeApplyConfig(new HashMap<>(config));
    }

    /** 音量をライブ反映する (KrkrzAndroidSetVolume)。value は 0-100。 */
    public static void setVolume(String key, int value) {
        if (key == null) return;
        nativeSetVolume(key, value);
    }

    // ------- native から呼ばれるコールバック (TJS スレッド) -------

    /** native (TJS スレッド) から呼ばれる。メインスレッドへ転送する。 */
    @SuppressWarnings("unused")
    static void onConfigReady(final Map<String, Object> config, final Map<String, Object> volumes) {
        Log.i(TAG, "onConfigReady: config="
                + (config == null ? "null" : config.size() + " entries")
                + " volumes="
                + (volumes == null ? "null" : volumes.size() + " entries"));
        cachedConfig = config;
        cachedVolumes = volumes;
        final Listener l = pendingListener;
        MAIN.post(() -> {
            if (l != null) l.onConfigReady(config, volumes);
        });
    }

    // ------- native methods -------

    private static native void nativeRequestConfig();
    private static native void nativeApplyConfig(Map<String, Object> config);
    private static native void nativeSetVolume(String key, int value);
}
