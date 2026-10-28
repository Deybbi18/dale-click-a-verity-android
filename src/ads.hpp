// Verity - anuncios.
//
// Android: anuncios reales de Google AdMob (banner abajo y anuncio con recompensa).
// Escritorio (Windows/Linux): AdMob no existe, así que el anuncio con recompensa se SIMULA con
// una pantalla de prueba de 3 segundos. Sirve para probar el flujo del juego. No genera dinero.
#pragma once

namespace verity::ads {

void init();
void shutdown();

// ¿Hay un anuncio con recompensa listo para mostrarse?
bool rewardedReady();
// Pide mostrar el anuncio con recompensa.
void showRewarded();
// Devuelve true UNA sola vez cuando la persona vio el anuncio completo y se gana la recompensa.
bool pollReward();
// Hay un anuncio en pantalla.
bool isShowing();

// Solo escritorio: el anuncio es de mentira y el juego dibuja una pantalla de prueba.
bool isSimulated();
float simProgress();  // 0..1

// Banner fijo en la parte de abajo (solo Android).
void setBannerVisible(bool visible);
// Altura que el juego deja libre abajo para el banner, en unidades lógicas (720 de ancho).
int bannerHeight();

}  // namespace verity::ads
