package com.verity.game;

import android.app.NativeActivity;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import com.google.android.gms.ads.AdError;
import com.google.android.gms.ads.AdRequest;
import com.google.android.gms.ads.AdSize;
import com.google.android.gms.ads.AdView;
import com.google.android.gms.ads.FullScreenContentCallback;
import com.google.android.gms.ads.LoadAdError;
import com.google.android.gms.ads.MobileAds;
import com.google.android.gms.ads.rewarded.RewardedAd;
import com.google.android.gms.ads.rewarded.RewardedAdLoadCallback;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Actividad de Verity. El juego está escrito en C++ (NativeActivity), pero los anuncios de Google AdMob
 * solo se pueden pedir desde Java, así que esta clase los maneja y el código C++ (src/ads_android.cpp)
 * llama a los métodos públicos de abajo.
 *
 * - Banner: una franja fija en la parte de abajo.
 * - Anuncio con recompensa: la persona lo ve completo y recibe un bono x2.
 */
public class MainActivity extends NativeActivity {
    private static final String TAG = "VerityAds";
    private static final long RETRY_DELAY_MS = 15_000;

    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private final AtomicBoolean rewardPending = new AtomicBoolean(false);

    private volatile RewardedAd rewardedAd;
    private volatile boolean showing;
    private boolean loading;
    private AdView banner;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        MobileAds.initialize(this, status -> runOnUiThread(this::loadRewarded));
        createBanner();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            // Pantalla completa sin botones del sistema (se muestran al deslizar desde el borde).
            getWindow().getDecorView().setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                            | View.SYSTEM_UI_FLAG_FULLSCREEN
                            | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (banner != null) banner.resume();
    }

    @Override
    protected void onPause() {
        if (banner != null) banner.pause();
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        if (banner != null) banner.destroy();
        super.onDestroy();
    }

    // ------------------------------------------------------------------ banner

    private void createBanner() {
        banner = new AdView(this);
        banner.setAdUnitId(getString(R.string.admob_banner_id));
        banner.setAdSize(AdSize.BANNER); // 320 x 50 dp
        FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT,
                Gravity.BOTTOM | Gravity.CENTER_HORIZONTAL);
        addContentView(banner, lp);
        banner.loadAd(new AdRequest.Builder().build());
    }

    /** Muestra u oculta el banner. Lo llama el código C++. */
    public void setBannerVisible(final boolean visible) {
        runOnUiThread(() -> {
            if (banner != null) banner.setVisibility(visible ? View.VISIBLE : View.GONE);
        });
    }

    // ------------------------------------------------------------------ anuncio con recompensa

    private void loadRewarded() {
        if (loading || rewardedAd != null) return;
        loading = true;
        RewardedAd.load(this, getString(R.string.admob_rewarded_id), new AdRequest.Builder().build(),
                new RewardedAdLoadCallback() {
                    @Override
                    public void onAdLoaded(RewardedAd ad) {
                        rewardedAd = ad;
                        loading = false;
                    }

                    @Override
                    public void onAdFailedToLoad(LoadAdError error) {
                        Log.w(TAG, "No se pudo cargar el anuncio: " + error.getMessage());
                        rewardedAd = null;
                        loading = false;
                        // Sin internet o sin anuncios disponibles: reintenta en un rato.
                        mainHandler.postDelayed(MainActivity.this::loadRewarded, RETRY_DELAY_MS);
                    }
                });
    }

    /** ¿Hay un anuncio listo? Lo llama el código C++. */
    public boolean isRewardedReady() {
        return rewardedAd != null && !showing;
    }

    /** Muestra el anuncio con recompensa. Lo llama el código C++. */
    public void showRewardedAd() {
        runOnUiThread(() -> {
            final RewardedAd ad = rewardedAd;
            if (ad == null || showing) {
                loadRewarded();
                return;
            }
            rewardedAd = null;
            showing = true;
            ad.setFullScreenContentCallback(new FullScreenContentCallback() {
                @Override
                public void onAdDismissedFullScreenContent() {
                    showing = false;
                    loadRewarded(); // deja listo el siguiente
                }

                @Override
                public void onAdFailedToShowFullScreenContent(AdError error) {
                    Log.w(TAG, "No se pudo mostrar el anuncio: " + error.getMessage());
                    showing = false;
                    loadRewarded();
                }
            });
            // Este bloque se ejecuta solo si la persona vio el anuncio completo.
            ad.show(MainActivity.this, rewardItem -> rewardPending.set(true));
        });
    }

    /** Devuelve true una sola vez por cada recompensa ganada. Lo llama el código C++. */
    public boolean consumeReward() {
        return rewardPending.getAndSet(false);
    }

    /** ¿Hay un anuncio en pantalla? Lo llama el código C++. */
    public boolean isAdShowing() {
        return showing;
    }
}
