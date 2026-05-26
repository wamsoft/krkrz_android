package jp.wamsoft.krkrz.ui;

import android.os.Bundle;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

import jp.wamsoft.krkrz.R;
import jp.wamsoft.krkrz.license.LicenseBridge;

/**
 * 権利情報表示画面。LicenseBridge 経由で KrkrZ エンジンが保持する license 文字列
 * (内部の license.txt + バージョン情報を組み立てたもの) を取得してスクロール表示する。
 * 取得失敗 / KrkrZ 未初期化時は @string/license_placeholder のままになる。
 */
public class LicenseActivity extends AppCompatActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_license);
        if (getSupportActionBar() != null) {
            getSupportActionBar().setTitle(R.string.menu_license);
            getSupportActionBar().setDisplayHomeAsUpEnabled(true);
        }

        TextView tv = findViewById(R.id.license_text);
        String text = LicenseBridge.getLicenseText();
        if (text != null && !text.isEmpty()) {
            tv.setText(text);
        }
        // 空文字なら layout のデフォルト (license_placeholder) のまま
    }

    @Override
    public boolean onSupportNavigateUp() {
        finish();
        return true;
    }
}
