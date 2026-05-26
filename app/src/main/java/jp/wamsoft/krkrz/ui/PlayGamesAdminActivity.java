package jp.wamsoft.krkrz.ui;

import android.os.Bundle;
import androidx.appcompat.app.AppCompatActivity;
import jp.wamsoft.krkrz.R;

public class PlayGamesAdminActivity extends AppCompatActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_playgames);
        if (getSupportActionBar() != null) {
            getSupportActionBar().setTitle(R.string.menu_playgames);
            getSupportActionBar().setDisplayHomeAsUpEnabled(true);
        }
    }

    @Override
    public boolean onSupportNavigateUp() {
        finish();
        return true;
    }
}
