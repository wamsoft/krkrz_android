package jp.wamsoft.krkrz.ui;

import android.os.Bundle;
import androidx.appcompat.app.AppCompatActivity;
import jp.wamsoft.krkrz.R;

public class LicenseActivity extends AppCompatActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_license);
        if (getSupportActionBar() != null) {
            getSupportActionBar().setTitle(R.string.menu_license);
            getSupportActionBar().setDisplayHomeAsUpEnabled(true);
        }
    }

    @Override
    public boolean onSupportNavigateUp() {
        finish();
        return true;
    }
}
