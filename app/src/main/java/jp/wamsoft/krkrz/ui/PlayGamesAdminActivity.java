package jp.wamsoft.krkrz.ui;

import android.os.Bundle;
import android.text.format.DateFormat;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

import com.google.android.gms.common.data.DataBuffer;
import com.google.android.gms.games.AnnotatedData;
import com.google.android.gms.games.GamesSignInClient;
import com.google.android.gms.games.PlayGames;
import com.google.android.gms.games.Player;
import com.google.android.gms.games.achievement.Achievement;
import com.google.android.gms.games.achievement.AchievementBuffer;
import com.google.android.gms.games.snapshot.SnapshotMetadata;
import com.google.android.gms.games.snapshot.SnapshotMetadataBuffer;
import com.google.android.material.button.MaterialButton;

import java.util.Date;

import jp.wamsoft.krkrz.R;

/**
 * Google Play Games 管理画面。
 * 既存の playgames プラグインは TJS 連動用 (callbackId + native post) なので、この
 * Activity では Play Games SDK の Task API を Activity から直接叩く構成にする。
 *
 * 表示内容:
 *  - サインイン状態 (isAuthenticated + getCurrentPlayer)、未認証時はサインインボタン
 *  - 実績一覧 (AchievementsClient.load)
 *  - セーブデータ一覧 (SnapshotsClient.load)
 */
public class PlayGamesAdminActivity extends AppCompatActivity {

    private TextView signinStatus;
    private MaterialButton signinButton;
    private LinearLayout achievementsList;
    private LinearLayout snapshotsList;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_playgames);
        if (getSupportActionBar() != null) {
            getSupportActionBar().setTitle(R.string.menu_playgames);
            getSupportActionBar().setDisplayHomeAsUpEnabled(true);
        }

        signinStatus     = findViewById(R.id.signin_status);
        signinButton     = findViewById(R.id.signin_button);
        achievementsList = findViewById(R.id.achievements_list);
        snapshotsList    = findViewById(R.id.snapshots_list);

        signinButton.setOnClickListener(v -> doSignIn());
        ((MaterialButton) findViewById(R.id.achievements_refresh))
                .setOnClickListener(v -> refreshAchievements());
        ((MaterialButton) findViewById(R.id.snapshots_refresh))
                .setOnClickListener(v -> refreshSnapshots());

        refreshSignIn();
        refreshAchievements();
        refreshSnapshots();
    }

    @Override
    public boolean onSupportNavigateUp() {
        finish();
        return true;
    }

    // ------------------------------------------------------------------
    // Sign-in
    // ------------------------------------------------------------------

    private void refreshSignIn() {
        signinStatus.setText(R.string.pg_signin_checking);
        signinButton.setVisibility(View.GONE);
        try {
            GamesSignInClient client = PlayGames.getGamesSignInClient(this);
            client.isAuthenticated().addOnCompleteListener(task -> {
                if (task.isSuccessful() && task.getResult() != null
                        && task.getResult().isAuthenticated()) {
                    fetchPlayerInfo();
                } else {
                    signinStatus.setText(R.string.pg_signin_not_signed_in);
                    signinButton.setVisibility(View.VISIBLE);
                }
            });
        } catch (Throwable t) {
            signinStatus.setText(getString(R.string.pg_sdk_error, messageOf(t)));
        }
    }

    private void fetchPlayerInfo() {
        try {
            PlayGames.getPlayersClient(this).getCurrentPlayer()
                    .addOnCompleteListener(task -> {
                        if (task.isSuccessful() && task.getResult() != null) {
                            Player p = task.getResult();
                            String name = p.getDisplayName();
                            String pid  = p.getPlayerId();
                            String detail = (name != null ? name : "") +
                                    " (" + (pid != null ? pid : "") + ")";
                            signinStatus.setText(getString(R.string.pg_signin_authenticated, detail));
                        } else {
                            signinStatus.setText(R.string.pg_signin_authenticated_no_player);
                        }
                        signinButton.setVisibility(View.GONE);
                    });
        } catch (Throwable t) {
            signinStatus.setText(getString(R.string.pg_sdk_error, messageOf(t)));
        }
    }

    private void doSignIn() {
        signinButton.setEnabled(false);
        try {
            PlayGames.getGamesSignInClient(this).signIn()
                    .addOnCompleteListener(task -> {
                        signinButton.setEnabled(true);
                        refreshSignIn();
                    });
        } catch (Throwable t) {
            signinButton.setEnabled(true);
            signinStatus.setText(getString(R.string.pg_sdk_error, messageOf(t)));
        }
    }

    // ------------------------------------------------------------------
    // Achievements
    // ------------------------------------------------------------------

    private void refreshAchievements() {
        achievementsList.removeAllViews();
        addInfoRow(achievementsList, getString(R.string.pg_loading));
        try {
            PlayGames.getAchievementsClient(this).load(false)
                    .addOnCompleteListener(this::onAchievementsLoaded);
        } catch (Throwable t) {
            achievementsList.removeAllViews();
            addInfoRow(achievementsList, getString(R.string.pg_sdk_error, messageOf(t)));
        }
    }

    private void onAchievementsLoaded(com.google.android.gms.tasks.Task<AnnotatedData<AchievementBuffer>> task) {
        achievementsList.removeAllViews();
        if (!task.isSuccessful() || task.getResult() == null) {
            addInfoRow(achievementsList, getString(R.string.pg_load_failed, messageOf(task.getException())));
            return;
        }
        AchievementBuffer buf = task.getResult().get();
        try {
            if (buf == null || buf.getCount() == 0) {
                addInfoRow(achievementsList, getString(R.string.pg_achievements_empty));
                return;
            }
            for (int i = 0; i < buf.getCount(); ++i) {
                Achievement a = buf.get(i);
                String stateLabel;
                switch (a.getState()) {
                    case Achievement.STATE_UNLOCKED: stateLabel = getString(R.string.pg_achievements_state_unlocked); break;
                    case Achievement.STATE_HIDDEN:   stateLabel = getString(R.string.pg_achievements_state_hidden); break;
                    default:                          stateLabel = getString(R.string.pg_achievements_state_revealed); break;
                }
                String title = a.getName();
                if (a.getType() == Achievement.TYPE_INCREMENTAL) {
                    title += " (" + a.getCurrentSteps() + "/" + a.getTotalSteps() + ")";
                }
                String desc = (a.getDescription() != null ? a.getDescription() : "") +
                        " — " + stateLabel;
                addItemRow(achievementsList, title, desc);
            }
        } finally {
            try { ((DataBuffer<?>) buf).release(); } catch (Throwable ignore) {}
        }
    }

    // ------------------------------------------------------------------
    // Snapshots
    // ------------------------------------------------------------------

    private void refreshSnapshots() {
        snapshotsList.removeAllViews();
        addInfoRow(snapshotsList, getString(R.string.pg_loading));
        try {
            PlayGames.getSnapshotsClient(this).load(false)
                    .addOnCompleteListener(this::onSnapshotsLoaded);
        } catch (Throwable t) {
            snapshotsList.removeAllViews();
            addInfoRow(snapshotsList, getString(R.string.pg_sdk_error, messageOf(t)));
        }
    }

    private void onSnapshotsLoaded(com.google.android.gms.tasks.Task<AnnotatedData<SnapshotMetadataBuffer>> task) {
        snapshotsList.removeAllViews();
        if (!task.isSuccessful() || task.getResult() == null) {
            addInfoRow(snapshotsList, getString(R.string.pg_load_failed, messageOf(task.getException())));
            return;
        }
        SnapshotMetadataBuffer buf = task.getResult().get();
        try {
            if (buf == null || buf.getCount() == 0) {
                addInfoRow(snapshotsList, getString(R.string.pg_snapshots_empty));
                return;
            }
            for (int i = 0; i < buf.getCount(); ++i) {
                SnapshotMetadata m = buf.get(i);
                String title = m.getUniqueName();
                CharSequence ts = DateFormat.format("yyyy-MM-dd HH:mm",
                        new Date(m.getLastModifiedTimestamp()));
                String desc = (m.getDescription() != null ? m.getDescription() : "") + " — " + ts;
                addItemRow(snapshotsList, title, desc);
            }
        } finally {
            try { buf.release(); } catch (Throwable ignore) {}
        }
    }

    // ------------------------------------------------------------------
    // Helpers
    // ------------------------------------------------------------------

    private void addInfoRow(LinearLayout parent, String text) {
        TextView tv = new TextView(this);
        tv.setText(text);
        tv.setPadding(8, 8, 8, 8);
        parent.addView(tv);
    }

    private void addItemRow(LinearLayout parent, String title, String desc) {
        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.VERTICAL);
        row.setPadding(8, 12, 8, 12);

        TextView t = new TextView(this);
        t.setText(title);
        t.setTextAppearance(android.R.style.TextAppearance_Medium);
        t.setTextIsSelectable(true);
        row.addView(t);

        TextView d = new TextView(this);
        d.setText(desc);
        d.setTextAppearance(android.R.style.TextAppearance_Small);
        d.setTextIsSelectable(true);
        row.addView(d);

        parent.addView(row);
    }

    private static String messageOf(Throwable t) {
        if (t == null) return "(unknown)";
        String m = t.getMessage();
        return (m != null && !m.isEmpty()) ? m : t.getClass().getSimpleName();
    }
}
