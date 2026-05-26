package jp.wamsoft.krkrz.ui;

import android.os.Bundle;

import androidx.appcompat.app.AppCompatActivity;

import com.google.android.material.switchmaterial.SwitchMaterial;

import jp.wamsoft.krkrz.R;
import jp.wamsoft.krkrz.virtualpad.VirtualPadPrefs;

/**
 * 機能設定画面。Phase 3 で仮想パッドの有効化 / 半透明表示を扱う。
 * SwitchMaterial 変更時は {@link VirtualPadPrefs} を介して SharedPreferences への
 * 永続化と native (VirtualPadBridge) への即時反映を同時に行う。
 */
public class SettingsActivity extends AppCompatActivity {
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
    }

    @Override
    public boolean onSupportNavigateUp() {
        finish();
        return true;
    }
}
