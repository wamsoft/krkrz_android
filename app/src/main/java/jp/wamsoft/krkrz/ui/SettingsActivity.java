package jp.wamsoft.krkrz.ui;

import android.os.Bundle;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

import com.google.android.material.slider.Slider;
import com.google.android.material.switchmaterial.SwitchMaterial;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

import jp.wamsoft.krkrz.R;
import jp.wamsoft.krkrz.config.KrkrzConfig;
import jp.wamsoft.krkrz.virtualpad.VirtualPadPrefs;

/**
 * 機能設定画面。
 *
 * <p>前半 (XML) は仮想パッドの ON/OFF など Android 固有設定。
 * 後半は {@link KrkrzConfig} 経由で吉里吉里本体 (TJS) から取得した
 * 「コンフィグ項目」を実行時に動的構築する。
 * <ul>
 *   <li>取得は非同期。{@code config_container} にまず「読み込み中」を出し、
 *       {@link KrkrzConfig.Listener#onConfigReady} で実際の行に差し替える。</li>
 *   <li>音量スライダーは操作のたびに {@link KrkrzConfig#setVolume} でライブ反映。</li>
 *   <li>音量以外の項目は編集値を {@link #editedConfig} に貯め、画面を閉じる
 *       ({@link #onPause}) ときに {@link KrkrzConfig#applyConfig} で一括通知する。</li>
 * </ul>
 */
public class SettingsActivity extends AppCompatActivity implements KrkrzConfig.Listener {

    // 編集中のコンフィグ値 (音量以外)。閉じるときに applyConfig でまとめて本体へ返す。
    private final Map<String, Object> editedConfig = new LinkedHashMap<>();
    private boolean haveConfig = false;
    private LinearLayout container;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_settings);
        if (getSupportActionBar() != null) {
            getSupportActionBar().setTitle(R.string.menu_settings);
            getSupportActionBar().setDisplayHomeAsUpEnabled(true);
        }

        SwitchMaterial swEnabled = findViewById(R.id.switch_vpad_enabled);
        swEnabled.setChecked(VirtualPadPrefs.isEnabled(this));
        swEnabled.setOnCheckedChangeListener((btn, checked) ->
                VirtualPadPrefs.setEnabled(this, checked));

        SwitchMaterial swTranslucent = findViewById(R.id.switch_vpad_translucent);
        swTranslucent.setChecked(VirtualPadPrefs.isTranslucent(this));
        swTranslucent.setOnCheckedChangeListener((btn, checked) ->
                VirtualPadPrefs.setTranslucent(this, checked));

        constrainContentWidth();

        container = findViewById(R.id.config_container);
        showLoading();
        KrkrzConfig.requestConfig(this);
    }

    /**
     * タブレット横向きなど広い画面で、設定行が画面いっぱいに伸びて間延びするのを防ぐ。
     * コンテンツ列の幅を {@code settings_content_max_width} で頭打ちにし中央寄せする
     * (それより狭い画面では従来どおり全幅)。回転時は Activity 再生成で再評価される。
     */
    private void constrainContentWidth() {
        View content = findViewById(R.id.settings_content);
        if (!(content.getLayoutParams() instanceof FrameLayout.LayoutParams)) return;
        int maxWidth = getResources().getDimensionPixelSize(R.dimen.settings_content_max_width);
        int screenWidth = getResources().getDisplayMetrics().widthPixels;
        FrameLayout.LayoutParams lp = (FrameLayout.LayoutParams) content.getLayoutParams();
        lp.width = (screenWidth > maxWidth)
                ? maxWidth
                : FrameLayout.LayoutParams.MATCH_PARENT;
        lp.gravity = Gravity.CENTER_HORIZONTAL;
        content.setLayoutParams(lp);
    }

    @Override
    protected void onPause() {
        super.onPause();
        // 画面を離れるタイミングで本体へまとめて通知。
        if (haveConfig && !editedConfig.isEmpty()) {
            KrkrzConfig.applyConfig(editedConfig);
        }
    }

    @Override
    public boolean onSupportNavigateUp() {
        finish();
        return true;
    }

    // =====================================================================
    // KrkrzConfig.Listener
    // =====================================================================

    @Override
    public void onConfigReady(Map<String, Object> config, Map<String, Object> volumes) {
        container.removeAllViews();
        if (config == null) {
            // 本体側に取得関数 (KrkrzAndroidGetConfig) が未実装。
            addCaption(container, getString(R.string.settings_cfg_unavailable));
            return;
        }
        haveConfig = true;
        buildSystemSection(config);
        buildMessageSection(config);
        buildVolumeSection(config, volumes);
        buildLanguageSection(config);
    }

    // =====================================================================
    // セクション構築
    // =====================================================================

    private void buildSystemSection(Map<String, Object> c) {
        addHeader(container, R.string.settings_cfg_sec_system);
        addSwitch(c, "screenEffect", R.string.settings_cfg_screen_effect);
        addSwitch(c, "systemEffect", R.string.settings_cfg_system_effect);
        addConfigSlider(c, "windowOpacity", R.string.settings_cfg_window_opacity, 0, 100);
    }

    private void buildMessageSection(Map<String, Object> c) {
        addHeader(container, R.string.settings_cfg_sec_message);
        addConfigSlider(c, "messageSpeed", R.string.settings_cfg_message_speed, 0, 100);
        addConfigSlider(c, "autoModeSpeed", R.string.settings_cfg_auto_speed, 0, 100);
        addSwitch(c, "autoReleaseOnChoice", R.string.settings_cfg_auto_release_choice);
        addSwitch(c, "autoWaitVoice", R.string.settings_cfg_auto_wait_voice);
        // スキップモード: 既読のみ(0) / すべて(1)
        addEnumDropdown(c, "skipMode", R.string.settings_cfg_skip_mode,
                new String[]{"0", "1"},
                new String[]{getString(R.string.settings_cfg_skip_existing),
                             getString(R.string.settings_cfg_skip_all)});
        addSwitch(c, "skipReleaseOnChoice", R.string.settings_cfg_skip_release_choice);
        addSwitch(c, "skipReadText", R.string.settings_cfg_skip_read_text);
    }

    private void buildVolumeSection(Map<String, Object> c, Map<String, Object> v) {
        addHeader(container, R.string.settings_cfg_sec_volume);
        addVolumeSlider(v, "master", getString(R.string.settings_cfg_vol_master));
        addVolumeSlider(v, "bgm", getString(R.string.settings_cfg_vol_bgm));
        addVolumeSlider(v, "se", getString(R.string.settings_cfg_vol_se));
        addVolumeSlider(v, "systemSe", getString(R.string.settings_cfg_vol_system_se));

        // ボイス (専用扱い)
        addHeader(container, R.string.settings_cfg_sec_voice);
        addVolumeSlider(v, "voice", getString(R.string.settings_cfg_vol_voice));
        addVolumeSlider(v, "systemVoice", getString(R.string.settings_cfg_vol_system_voice));
        addSwitch(c, "lowerBgmOnVoice", R.string.settings_cfg_lower_bgm_on_voice);
        addSwitch(c, "lowerSeOnVoice", R.string.settings_cfg_lower_se_on_voice);

        // キャラ別ボイス: charVoiceNames (改行区切り) からスライダーを動的生成。
        // 音量値は native が "charVoice:<name>" キーで volumes に詰めて返す。
        String names = asString(c, "charVoiceNames", "");
        if (!names.isEmpty()) {
            addCaption(container, getString(R.string.settings_cfg_char_voice));
            for (String raw : names.split("\\r?\\n")) {
                String name = raw.trim();
                if (name.isEmpty()) continue;
                addVolumeSlider(v, "charVoice:" + name, name);
            }
        }
    }

    private void buildLanguageSection(Map<String, Object> c) {
        boolean hasMain = c.containsKey("mainTextLang");
        boolean hasSub = c.containsKey("subTextLang");
        if (!hasMain && !hasSub) return;
        addHeader(container, R.string.settings_cfg_sec_language);
        if (hasMain) {
            addLangDropdown(c, "mainTextLang", "mainTextLangList",
                    R.string.settings_cfg_main_text);
        }
        if (hasSub) {
            addLangDropdown(c, "subTextLang", "subTextLangList",
                    R.string.settings_cfg_sub_text);
        }
    }

    // =====================================================================
    // 行ウィジェット生成ヘルパ
    // =====================================================================

    private void addSwitch(Map<String, Object> c, String key, int labelRes) {
        if (!c.containsKey(key)) return;
        boolean cur = asLong(c, key, 0) != 0;
        editedConfig.put(key, cur ? 1L : 0L);
        SwitchMaterial sw = new SwitchMaterial(this);
        sw.setText(labelRes);
        sw.setChecked(cur);
        sw.setLayoutParams(rowParams());
        sw.setOnCheckedChangeListener((b, checked) ->
                editedConfig.put(key, checked ? 1L : 0L));
        container.addView(sw);
    }

    /** 音量以外の数値スライダー (編集値を editedConfig に貯める)。 */
    private void addConfigSlider(Map<String, Object> c, String key, int labelRes,
                                 int min, int max) {
        if (!c.containsKey(key)) return;
        int cur = clamp((int) asLong(c, key, min), min, max);
        editedConfig.put(key, (long) cur);
        String label = getString(labelRes);
        TextView tv = addCaption(container, label + " (" + cur + ")");
        Slider slider = makeSlider(min, max, cur);
        slider.addOnChangeListener((s, value, fromUser) -> {
            int iv = Math.round(value);
            tv.setText(label + " (" + iv + ")");
            editedConfig.put(key, (long) iv);
        });
        container.addView(slider);
    }

    /** 音量スライダー (操作のたびにライブ反映)。 */
    private void addVolumeSlider(Map<String, Object> volumes, String volKey, String label) {
        if (volumes == null || !volumes.containsKey(volKey)) return;
        int cur = clamp(asInt(volumes, volKey, 100), 0, 100);
        TextView tv = addCaption(container, label + " (" + cur + ")");
        Slider slider = makeSlider(0, 100, cur);
        slider.addOnChangeListener((s, value, fromUser) -> {
            int iv = Math.round(value);
            tv.setText(label + " (" + iv + ")");
            if (fromUser) KrkrzConfig.setVolume(volKey, iv);
        });
        container.addView(slider);
    }

    /** 固定選択肢のドロップダウン (値は文字列で editedConfig へ Long 保存)。 */
    private void addEnumDropdown(Map<String, Object> c, String key, int labelRes,
                                 String[] values, String[] labels) {
        if (!c.containsKey(key)) return;
        String cur = String.valueOf(asLong(c, key, 0));
        editedConfig.put(key, asLong(c, key, 0));
        addCaption(container, getString(labelRes));
        Spinner spinner = makeSpinner(labels);
        int sel = indexOf(values, cur);
        if (sel >= 0) spinner.setSelection(sel);
        spinner.setOnItemSelectedListener(new SimpleSelected(pos ->
                editedConfig.put(key, parseLong(values[pos]))));
        container.addView(spinner);
    }

    /** 言語ドロップダウン。選択肢は本体メタデータ ({listKey} の改行/タブ区切り) から。 */
    private void addLangDropdown(Map<String, Object> c, String key, String listKey, int labelRes) {
        String cur = asString(c, key, "");
        List<String> values = new ArrayList<>();
        List<String> labels = new ArrayList<>();
        parseOptionList(asString(c, listKey, ""), values, labels);
        if (values.isEmpty()) {
            // 選択肢メタデータが無ければ現在値のみ表示。
            values.add(cur);
            labels.add(cur.isEmpty() ? "-" : cur);
        }
        editedConfig.put(key, cur);
        addCaption(container, getString(labelRes));
        Spinner spinner = makeSpinner(labels.toArray(new String[0]));
        int sel = values.indexOf(cur);
        if (sel >= 0) spinner.setSelection(sel);
        final List<String> vals = values;
        spinner.setOnItemSelectedListener(new SimpleSelected(pos ->
                editedConfig.put(key, vals.get(pos))));
        container.addView(spinner);
    }

    // =====================================================================
    // 低レベル View ファクトリ
    // =====================================================================

    private Slider makeSlider(int min, int max, int value) {
        Slider slider = new Slider(this);
        slider.setValueFrom(min);
        slider.setValueTo(max);
        slider.setStepSize(1);
        slider.setValue(clamp(value, min, max));
        slider.setLayoutParams(rowParams());
        return slider;
    }

    private Spinner makeSpinner(String[] labels) {
        Spinner spinner = new Spinner(this);
        ArrayAdapter<String> adapter = new ArrayAdapter<>(this,
                android.R.layout.simple_spinner_item, labels);
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spinner.setAdapter(adapter);
        spinner.setLayoutParams(rowParams());
        return spinner;
    }

    private void addHeader(ViewGroup parent, int titleRes) {
        TextView tv = new TextView(this);
        tv.setText(titleRes);
        tv.setTextAppearance(com.google.android.material.R.style.TextAppearance_Material3_TitleMedium);
        LinearLayout.LayoutParams lp = rowParams();
        lp.topMargin = dp(16);
        tv.setLayoutParams(lp);
        parent.addView(tv);
    }

    private TextView addCaption(ViewGroup parent, String text) {
        TextView tv = new TextView(this);
        tv.setText(text);
        tv.setTextAppearance(com.google.android.material.R.style.TextAppearance_Material3_BodySmall);
        tv.setLayoutParams(rowParams());
        parent.addView(tv);
        return tv;
    }

    private void showLoading() {
        container.removeAllViews();
        TextView tv = new TextView(this);
        tv.setText(R.string.settings_cfg_loading);
        tv.setGravity(Gravity.CENTER);
        tv.setLayoutParams(rowParams());
        container.addView(tv);
    }

    // =====================================================================
    // util
    // =====================================================================

    private LinearLayout.LayoutParams rowParams() {
        return new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT);
    }

    private int dp(int v) {
        return Math.round(TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v,
                getResources().getDisplayMetrics()));
    }

    private static int clamp(int v, int min, int max) {
        return v < min ? min : (v > max ? max : v);
    }

    private static int indexOf(String[] arr, String v) {
        for (int i = 0; i < arr.length; i++) if (arr[i].equals(v)) return i;
        return -1;
    }

    private static long parseLong(String s) {
        try { return Long.parseLong(s.trim()); } catch (NumberFormatException e) { return 0; }
    }

    private static long asLong(Map<String, Object> m, String key, long def) {
        Object o = m.get(key);
        if (o instanceof Number) return ((Number) o).longValue();
        if (o instanceof String) {
            try { return Long.parseLong(((String) o).trim()); } catch (NumberFormatException ignored) {}
        }
        return def;
    }

    private static int asInt(Map<String, Object> m, String key, int def) {
        return (int) asLong(m, key, def);
    }

    private static String asString(Map<String, Object> m, String key, String def) {
        Object o = m.get(key);
        return o != null ? o.toString() : def;
    }

    /** "value\tlabel" を改行区切りで並べたメタデータ文字列をパース。タブ無しは value==label。 */
    private static void parseOptionList(String src, List<String> values, List<String> labels) {
        if (src == null || src.isEmpty()) return;
        for (String raw : src.split("\\r?\\n")) {
            String line = raw.trim();
            if (line.isEmpty()) continue;
            int tab = line.indexOf('\t');
            if (tab >= 0) {
                values.add(line.substring(0, tab).trim());
                labels.add(line.substring(tab + 1).trim());
            } else {
                values.add(line);
                labels.add(line);
            }
        }
    }

    /** AdapterView.OnItemSelectedListener の薄いラッパ。 */
    private static final class SimpleSelected implements AdapterView.OnItemSelectedListener {
        interface Sel { void onSelect(int pos); }
        private final Sel sel;
        SimpleSelected(Sel sel) { this.sel = sel; }
        @Override public void onItemSelected(AdapterView<?> p, View v, int pos, long id) {
            sel.onSelect(pos);
        }
        @Override public void onNothingSelected(AdapterView<?> p) {}
    }
}
