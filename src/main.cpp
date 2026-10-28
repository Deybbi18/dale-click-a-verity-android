// Verity - un juego de clics con una bolita sonriente.
//
// Pantalla vertical de 720 unidades de ancho; el alto se adapta a la ventana. Los mismos controles
// sirven con el dedo (Android) y con el ratón (Windows).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "raylib.h"
#include "rlgl.h"

#include "ads.hpp"
#include "game.hpp"
#include "gfx.hpp"
#include "platform.hpp"

using namespace verity;
using namespace verity::gfx;

namespace {

constexpr float kW = 720.0f;       // ancho lógico
constexpr float kTopH = 170.0f;    // alto del marcador superior
constexpr float kTabH = 120.0f;    // alto de la barra de pestañas
constexpr float kRowH = 116.0f;    // alto de cada fila de mejora
constexpr float kRowGap = 12.0f;
constexpr float kCardW = 324.0f;   // tarjetas de skins
constexpr float kCardH = 380.0f;
constexpr float kArenaW = 660.0f;  // arena del minijuego
constexpr float kGameCardH = 320.0f;  // tarjetas del menú de minijuegos

enum class Tab { Play, Upgrades, Skins, Games, Goals };
enum class GamesMode { Menu, Catch, Wheel, Simon };
enum class AdPurpose { None, Boost, DoubleOffline, DoubleCatch, DoubleSimon, DoubleDaily };
constexpr int kTabCount = 5;

float frand(float a, float b) { return a + (b - a) * (static_cast<float>(GetRandomValue(0, 10000)) / 10000.0f); }
bool inside(Rectangle r, Vector2 p) { return p.x >= r.x && p.x <= r.x + r.width && p.y >= r.y && p.y <= r.y + r.height; }
float dist(Vector2 a, Vector2 b) { return std::hypot(a.x - b.x, a.y - b.y); }

struct Pointer {
  Vector2 pos{}, prev{}, pressPos{};
  bool down = false, pressed = false, released = false, dragged = false;
  float dy = 0;
};

struct Particle {
  Vector2 pos, vel;
  float life, maxLife, size;
  bool golden;
};

struct FloatText {
  Vector2 pos;
  float life, maxLife;
  std::string text;
  Color color;
  int size;
};

struct Scroll {
  float y = 0, vel = 0;
};

struct Layout {
  float sw = 450, sh = 800, H = 1280, scale = 1, offX = 0, offY = 0;
  float top = 0;            // margen superior seguro
  float contentTop = 0;
  float contentBottom = 0;
  float tabY = 0;
  float banner = 0;
};

// --- Sonidos generados con código (sin archivos de audio) ---
Sound makeTone(float f0, float f1, float seconds, float volume) {
  const int rate = 22050;
  const int frames = static_cast<int>(rate * seconds);
  short* data = static_cast<short*>(std::malloc(sizeof(short) * frames));
  double phase = 0;
  for (int i = 0; i < frames; ++i) {
    const float t = static_cast<float>(i) / frames;
    const float f = f0 + (f1 - f0) * t;
    phase += 2.0 * PI * f / rate;
    const float attack = std::min(1.0f, i / (rate * 0.004f));
    const float env = (1.0f - t) * (1.0f - t) * attack;
    data[i] = static_cast<short>(std::sin(phase) * volume * env * 32767.0f);
  }
  Wave w = {};
  w.frameCount = static_cast<unsigned>(frames);
  w.sampleRate = rate;
  w.sampleSize = 16;
  w.channels = 1;
  w.data = data;
  Sound s = LoadSoundFromWave(w);
  std::free(data);
  return s;
}

struct Sfx {
  bool ready = false;
  Sound pop[3] = {};
  Sound crit = {}, buy = {}, error = {}, win = {}, hit = {};
  int next = 0;

  void init() {
    InitAudioDevice();
    ready = IsAudioDeviceReady();
    if (!ready) return;
    pop[0] = makeTone(700, 1100, 0.07f, 0.45f);
    pop[1] = makeTone(780, 1200, 0.07f, 0.45f);
    pop[2] = makeTone(640, 1000, 0.07f, 0.45f);
    crit = makeTone(900, 1900, 0.20f, 0.5f);
    buy = makeTone(600, 1000, 0.14f, 0.5f);
    error = makeTone(220, 140, 0.16f, 0.5f);
    win = makeTone(500, 1300, 0.40f, 0.5f);
    hit = makeTone(1000, 1500, 0.08f, 0.5f);
  }
  void unload() {
    if (!ready) return;
    for (Sound& s : pop) UnloadSound(s);
    UnloadSound(crit);
    UnloadSound(buy);
    UnloadSound(error);
    UnloadSound(win);
    UnloadSound(hit);
    CloseAudioDevice();
    ready = false;
  }
};

struct App {
  Game game;
  Sfx sfx;
  Layout L;
  Pointer ptr;

  Tab tab = Tab::Play;
  GamesMode gmode = GamesMode::Menu;
  Scroll scrollUp, scrollSkin, scrollGames, scrollGoals;

  float time = 0;
  float squash = 0, happyT = 0, blinkT = 0, blinkWait = 2.5f;
  float passiveAcc = 0;
  std::vector<Particle> particles;
  std::vector<FloatText> texts;

  std::string toastText;
  float toastT = 0;

  bool offlineOpen = false;
  double offlineGain = 0;
  AdPurpose adPurpose = AdPurpose::None;
  bool catchDoubled = false;
  bool simonDoubled = false;
  int lastLitPad = -1;
  int64_t today = 0;
  float dayCheck = 0;
  bool modal = false;

  // Ruleta
  float wheelAngle = 0, wheelFrom = 0, wheelTo = 0, wheelT = 1.0f;
  bool wheelAnimating = false;
  std::string wheelResult;

  std::string savePath;
  float saveTimer = 8.0f;
  float ballPulse = 0;

  // ------------------------------------------------------------------ inicio
  void init() {
    SetRandomSeed(static_cast<unsigned>(platform::unixNow()));
    savePath = platform::savePath();
    const std::string data = platform::readFile(savePath);
    if (!data.empty()) {
      const LoadResult r = game.deserialize(data, platform::unixNow());
      if (r.ok && r.offlineGain > 0) {
        offlineGain = r.offlineGain;
        offlineOpen = true;
      }
    }
    today = platform::dayNumber();
    game.refreshMissions(today);
    if (game.dailyAvailable(today) && !offlineOpen) say("¡Tienes una recompensa diaria!");
    sfx.init();
    ads::init();
    ads::setBannerVisible(true);
  }

  void shutdown() {
    saveNow();
    sfx.unload();
    ads::shutdown();
  }

  void saveNow() { platform::writeFileAtomic(savePath, game.serialize(platform::unixNow())); }

  void play(Sound s, float pitch = 1.0f) {
    if (!sfx.ready || !game.soundOn) return;
    SetSoundPitch(s, pitch);
    PlaySound(s);
  }

  void say(const std::string& text) {
    toastText = text;
    toastT = 2.6f;
  }

  // ------------------------------------------------------------------ entrada y diseño
  void computeLayout() {
    L.sw = static_cast<float>(GetScreenWidth());
    L.sh = static_cast<float>(GetScreenHeight());
    L.H = std::clamp(kW * L.sh / std::max(1.0f, L.sw), 1000.0f, 1700.0f);
    L.scale = std::min(L.sw / kW, L.sh / L.H);
    L.offX = (L.sw - kW * L.scale) / 2;
    L.offY = (L.sh - L.H * L.scale) / 2;
    L.top = platform::safeTop();
    L.banner = static_cast<float>(ads::bannerHeight());
    L.tabY = L.H - L.banner - kTabH;
    L.contentTop = L.top + kTopH;
    L.contentBottom = L.tabY;
  }

  void updatePointer() {
    const Vector2 m = GetMousePosition();
    ptr.prev = ptr.pos;
    ptr.pos = {(m.x - L.offX) / L.scale, (m.y - L.offY) / L.scale};
    ptr.pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    ptr.released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    ptr.down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    if (ptr.pressed) {
      ptr.pressPos = ptr.pos;
      ptr.prev = ptr.pos;
      ptr.dragged = false;
    }
    ptr.dy = ptr.down ? ptr.pos.y - ptr.prev.y : 0.0f;
    if (ptr.down && dist(ptr.pos, ptr.pressPos) > 18.0f) ptr.dragged = true;
  }

  // Un toque válido: empezó y terminó dentro del rectángulo, sin arrastrar.
  bool tapped(Rectangle r, bool ignoreModal = false) const {
    if (modal && !ignoreModal) return false;
    return ptr.released && !ptr.dragged && inside(r, ptr.pressPos) && inside(r, ptr.pos);
  }
  bool inView(Vector2 p) const { return p.y >= L.contentTop && p.y <= L.contentBottom; }
  bool tappedInView(Rectangle r) const { return tapped(r) && inView(ptr.pos) && inView(ptr.pressPos); }
  bool holding(Rectangle r) const { return !modal && ptr.down && !ptr.dragged && inside(r, ptr.pressPos) && inside(r, ptr.pos); }

  void updateScroll(Scroll& s, float contentH, float dt) {
    const float viewH = L.contentBottom - L.contentTop;
    const float maxY = std::max(0.0f, contentH - viewH);
    if (!modal && ptr.down && ptr.dragged && inView(ptr.pressPos)) {
      s.y -= ptr.dy;
      s.vel = -ptr.dy / std::max(dt, 0.001f) * 0.6f + s.vel * 0.4f;
    } else {
      s.y += s.vel * dt;
      s.vel *= std::pow(0.03f, dt);
      if (std::fabs(s.vel) < 5.0f) s.vel = 0;
    }
    s.y -= GetMouseWheelMove() * 90.0f;
    if (s.y < 0) { s.y = 0; s.vel = 0; }
    if (s.y > maxY) { s.y = maxY; s.vel = 0; }
  }

  // ------------------------------------------------------------------ efectos
  void burst(Vector2 from, int n, float angleMin, float angleMax, float speedMin, float speedMax, float sizeMin,
             float sizeMax, bool golden = false) {
    for (int i = 0; i < n; ++i) {
      const float a = frand(angleMin, angleMax) * DEG2RAD;
      const float sp = frand(speedMin, speedMax);
      Particle p;
      p.pos = from;
      p.vel = {std::cos(a) * sp, std::sin(a) * sp};
      p.maxLife = p.life = frand(1.0f, 1.6f);
      p.size = frand(sizeMin, sizeMax);
      p.golden = golden;
      particles.push_back(p);
    }
    if (particles.size() > 170) particles.erase(particles.begin(), particles.begin() + (particles.size() - 170));
  }

  void floatText(Vector2 pos, const std::string& text, Color color, int size) {
    texts.push_back({pos, 0.9f, 0.9f, text, color, size});
    if (texts.size() > 24) texts.erase(texts.begin());
  }

  void updateEffects(float dt) {
    for (Particle& p : particles) {
      p.vel.y += 1100.0f * dt;
      p.pos.x += p.vel.x * dt;
      p.pos.y += p.vel.y * dt;
      p.life -= dt;
    }
    particles.erase(std::remove_if(particles.begin(), particles.end(), [](const Particle& p) { return p.life <= 0; }),
                    particles.end());
    for (FloatText& t : texts) {
      t.pos.y -= 110.0f * dt;
      t.life -= dt;
    }
    texts.erase(std::remove_if(texts.begin(), texts.end(), [](const FloatText& t) { return t.life <= 0; }), texts.end());

    squash = std::max(0.0f, squash - dt * 5.5f);
    happyT = std::max(0.0f, happyT - dt);
    toastT = std::max(0.0f, toastT - dt);
    blinkWait -= dt;
    if (blinkWait <= 0) {
      blinkT = 0.16f;
      blinkWait = frand(2.0f, 5.0f);
    }
    blinkT = std::max(0.0f, blinkT - dt);
  }

  // ------------------------------------------------------------------ anuncios
  void requestAd(AdPurpose purpose) {
    if (!ads::rewardedReady()) {
      say("El anuncio todavía no está listo. Prueba en unos segundos.");
      play(sfx.error);
      return;
    }
    adPurpose = purpose;
    ads::showRewarded();
  }

  void applyReward() {
    switch (adPurpose) {
      case AdPurpose::Boost:
        game.grantAdBoost();
        say("¡Bono x2 durante 3 minutos!");
        break;
      case AdPurpose::DoubleOffline:
        game.grantBonus(offlineGain);
        say("¡Duplicaste lo que ganaste! +" + formatNumber(offlineGain));
        offlineOpen = false;
        break;
      case AdPurpose::DoubleCatch:
        game.grantBonus(game.catchGame.reward);
        catchDoubled = true;
        say("¡Premio duplicado! +" + formatNumber(game.catchGame.reward));
        break;
      case AdPurpose::DoubleSimon:
        game.grantBonus(game.simon.reward);
        simonDoubled = true;
        say("¡Premio duplicado! +" + formatNumber(game.simon.reward));
        break;
      case AdPurpose::DoubleDaily: {
        const double got = game.claimDaily(today, true);
        if (got > 0) {
          say("¡Recompensa diaria x2! +" + formatNumber(got));
          burst({kW / 2, L.contentTop + 200}, 14, -170, -10, 300, 700, 14, 24, true);
        }
        break;
      }
      case AdPurpose::None:
        break;
    }
    adPurpose = AdPurpose::None;
    play(sfx.win);
    saveNow();
  }

  // ------------------------------------------------------------------ ciclo
  void frame(float rawDt) {
    const float dt = std::min(rawDt, 0.1f);
    time += dt;
    computeLayout();
    updatePointer();

    if (ads::pollReward()) applyReward();
    const bool adUp = ads::isShowing();
    modal = offlineOpen || (adUp && ads::isSimulated());

    if (!adUp) {
      game.tick(dt);
      game.updateCatch(dt);
      game.updateSimon(dt);
      updateWheel(dt);
      updateDayAndMedals(dt);
      updateEffects(dt);
      saveTimer -= dt;
      if (saveTimer <= 0) {
        saveNow();
        saveTimer = 8.0f;
      }
    }
    if (game.clicks >= 3) game.hintSeen = true;

    BeginDrawing();
    DrawRectangleGradientV(0, 0, static_cast<int>(L.sw), static_cast<int>(L.sh), kSkyTop, kSkyBottom);

    Camera2D cam = {};
    cam.offset = {L.offX, L.offY};
    cam.zoom = L.scale;
    BeginMode2D(cam);

    switch (tab) {
      case Tab::Play: drawPlay(dt); break;
      case Tab::Upgrades: drawUpgrades(dt); break;
      case Tab::Skins: drawSkins(dt); break;
      case Tab::Games: drawGames(dt); break;
      case Tab::Goals: drawGoals(dt); break;
    }
    drawHud();
    drawTabBar();
    drawEffects();
    drawToast();
    if (offlineOpen) drawOfflinePopup();
    if (adUp && ads::isSimulated()) drawSimAd();

    EndMode2D();
    EndDrawing();
  }

  // Cambio de día (misiones nuevas) y medallas recién ganadas.
  void updateDayAndMedals(float dt) {
    dayCheck -= dt;
    if (dayCheck <= 0) {
      dayCheck = 5.0f;
      const int64_t d = platform::dayNumber();
      if (d != today) {
        today = d;
        game.refreshMissions(today);
        if (game.dailyAvailable(today)) say("¡Nuevo día! Tu recompensa diaria te espera.");
        saveNow();
      }
    }
    const std::vector<int> fresh = game.checkAchievements();
    if (!fresh.empty()) {
      if (fresh.size() == 1) say(std::string("¡Medalla: ") + achievementDef(fresh[0]).name + "!  +2%");
      else say("¡" + std::to_string(fresh.size()) + " medallas nuevas!");
      play(sfx.win);
      burst({kW / 2, L.contentTop + 200}, 10, -170, -10, 300, 700, 14, 24, true);
      saveNow();
    }
  }

  // ------------------------------------------------------------------ marcador y pestañas
  void drawHud() {
    const float h = L.top + kTopH;
    DrawRectangleRec({0, 0, kW, h}, {255, 255, 255, 255});
    DrawRectangleRec({0, h, kW, 6}, {20, 60, 90, 30});

    const std::string count = formatNumber(std::floor(game.smiles));
    const int tw = textWidth(count.c_str(), 70);
    const float total = 64.0f + 14.0f + static_cast<float>(tw);
    const float x0 = kW / 2 - total / 2;
    const float cy = L.top + 66.0f;
    drawMiniSmiley({x0 + 32.0f, cy}, 32.0f, false, 1.0f);
    drawText(count.c_str(), static_cast<int>(x0 + 78.0f), static_cast<int>(cy - 33.0f), 70, kInk);

    const std::string info = "por clic " + formatNumber(game.perClick()) + "   por segundo " + formatNumber(game.perSecond());
    drawTextCentered(info.c_str(), kW / 2, L.top + 118.0f, 30, kInkSoft);

    // Bono x2
    if (game.boostLeft > 0) {
      const std::string t = "x2  " + formatTime(game.boostLeft);
      const int w = textWidth(t.c_str(), 30) + 36;
      const Rectangle chip = {20, L.top + 14, static_cast<float>(w), 48};
      drawRound(chip, 24, kOrange);
      drawText(t.c_str(), static_cast<int>(chip.x + 18), static_cast<int>(chip.y + 10), 30, WHITE);
    }

    // Sonido
    const Rectangle snd = {kW - 84, L.top + 8, 64, 64};
    drawRound(snd, 32, {235, 244, 250, 255});
    drawSpeaker({snd.x + 30, snd.y + 32}, 22, game.soundOn, kInk);
    if (tapped(snd)) {
      game.soundOn = !game.soundOn;
      saveNow();
    }
  }

  bool anyUpgradeAffordable() const {
    for (int i = 0; i < kUpgradeCount; ++i)
      if (game.canBuyUpgrade(i)) return true;
    return false;
  }
  bool anySkinAffordable() const {
    for (int i = 0; i < kSkinCount; ++i)
      if (!game.skinOwned[i] && game.smiles >= skinDef(i).cost) return true;
    return false;
  }

  bool tabsLocked() const {
    return (gmode == GamesMode::Catch && game.catchGame.active && !game.catchGame.finished) ||
           (gmode == GamesMode::Simon && game.simon.active && !game.simon.finished) || wheelAnimating;
  }

  void drawTabBar() {
    const Rectangle bar = {0, L.tabY, kW, kTabH};
    DrawRectangleRec({0, L.tabY - 6, kW, 6}, {20, 60, 90, 30});
    DrawRectangleRec(bar, {255, 255, 255, 255});
    DrawRectangleRec({0, L.tabY + kTabH, kW, L.banner}, {255, 255, 255, 255});

    static const char* const kNames[kTabCount] = {"Jugar", "Mejoras", "Skins", "Juegos", "Metas"};
    const float w = kW / kTabCount;
    for (int i = 0; i < kTabCount; ++i) {
      const Rectangle r = {w * i, L.tabY, w, kTabH};
      const bool active = static_cast<int>(tab) == i;
      const Vector2 ic = {r.x + w / 2, r.y + 44};
      if (active) drawRound({r.x + w / 2 - 44, r.y + 8, 88, 72}, 26, {255, 236, 160, 255});
      switch (i) {
        case 0: drawMiniSmiley(ic, 24, false, 1.0f); break;
        case 1: drawUpArrow(ic, 40, kGreen); break;
        case 2: drawHatIcon(ic, 42); break;
        case 3: drawWheelIcon(ic, 26); break;
        default: drawMedalIcon(ic, 46); break;
      }
      drawTextCentered(kNames[i], r.x + w / 2, r.y + 86, 20, active ? kInk : kInkSoft);

      const bool dot = (i == 1 && anyUpgradeAffordable()) || (i == 2 && anySkinAffordable()) ||
                       (i == 4 && (game.dailyAvailable(today) || game.missionsClaimable()));
      if (dot && !active) {
        DrawCircleV({r.x + w / 2 + 34, r.y + 14}, 11, WHITE);
        DrawCircleV({r.x + w / 2 + 34, r.y + 14}, 8, kRed);
      }
      if (tapped(r)) {
        if (tabsLocked()) {
          say("Termina el minijuego primero.");
        } else if (!active) {
          tab = static_cast<Tab>(i);
          scrollUp.vel = scrollSkin.vel = scrollGames.vel = scrollGoals.vel = 0;
        }
      }
    }
  }

  // ------------------------------------------------------------------ pantalla: jugar
  void clickBall(Vector2 bc, float br, Vector2 tapPos) {
    const ClickResult r = game.click();
    squash = 1.0f;
    happyT = 0.4f;
    const int n = r.crit ? 9 : (game.perClick() >= 50 ? 4 : 3);
    burst({bc.x, bc.y - br * 0.5f}, n, -140, -40, 380, 760, 14, 24, r.crit);
    const std::string t = (r.crit ? "¡x5!  +" : "+") + formatNumber(r.gained);
    floatText({tapPos.x + frand(-40.0f, 40.0f), tapPos.y - 30.0f + frand(-20.0f, 10.0f)}, t, r.crit ? kOrange : WHITE, r.crit ? 50 : 40);
    play(r.crit ? sfx.crit : sfx.pop[sfx.next++ % 3], frand(0.92f, 1.12f));
  }

  // La bolita deja sitio arriba para los gorros altos y abajo para el texto y el botón del anuncio.
  float ballRadius() const {
    const float contentH = L.contentBottom - L.contentTop;
    return std::clamp((contentH - 260.0f) / 2.8f, 90.0f, 200.0f);
  }
  Vector2 ballCenter() const {
    const float contentH = L.contentBottom - L.contentTop;
    const float br = ballRadius();
    return {kW / 2, L.contentTop + std::max(br * 1.8f + 10.0f, contentH * 0.40f)};
  }

  void drawPlay(float dt) {
    const Vector2 bc = ballCenter();
    const float br = ballRadius();

    // Sonrisas que salen solas cuando hay producción por segundo
    if (game.perSecond() > 0 && !modal) {
      passiveAcc += dt * std::min(7.0f, 1.0f + static_cast<float>(std::log10(game.perSecond() + 1.0)) * 1.3f);
      while (passiveAcc >= 1.0f) {
        passiveAcc -= 1.0f;
        burst({bc.x + frand(-br * 0.5f, br * 0.5f), bc.y - br * 0.7f}, 1, -125, -55, 260, 460, 12, 18);
      }
    }

    if (!modal && ptr.pressed && dist(ptr.pos, bc) <= br * 1.12f && ptr.pos.y > L.contentTop) clickBall(bc, br, ptr.pos);

    // Bolita
    const float bob = std::sin(time * 2.2f) * 7.0f;
    const float sx = 1.0f + 0.12f * squash, sy = 1.0f - 0.14f * squash;
    rlPushMatrix();
    rlTranslatef(bc.x, bc.y + bob + (1.0f - sy) * br, 0);
    rlScalef(sx, sy, 1);
    drawSmiley({0, 0}, br, game.skinEquipped, happyT > 0 ? 1.0f : 0.0f, blinkT > 0 ? 1.0f : 0.0f);
    rlPopMatrix();

    if (!game.hintSeen) {
      const float a = 0.55f + 0.45f * std::sin(time * 4.0f);
      drawTextCentered("¡Toca la bolita!", kW / 2, bc.y + br + 70, 40, withAlpha(kInk, a));
    } else if (game.critChance() > 0) {
      const std::string c = "Clic crítico: " + std::to_string(static_cast<int>(std::round(game.critChance() * 100))) + "%  (x5)";
      drawTextCentered(c.c_str(), kW / 2, bc.y + br + 64, 30, kInkSoft);
    }

    // Botón del anuncio con recompensa
    const Rectangle ad = {60, L.contentBottom - 140, 600, 104};
    if (game.canWatchAd()) {
      const bool pressed = holding(ad);
      const Rectangle face = drawButton(ad, kOrange, true, pressed);
      drawTextCentered("Ver anuncio: x2 sonrisas", face.x + face.width / 2, face.y + 20, 30, WHITE);
      const char* sub = ads::isSimulated() ? "durante 3 minutos  (anuncio de prueba)" : "durante 3 minutos";
      drawTextCentered(sub, face.x + face.width / 2, face.y + 62, 20, {255, 244, 225, 255});
      if (tapped(ad)) requestAd(AdPurpose::Boost);
    } else {
      drawRound(ad, 20, {255, 255, 255, 200});
      const std::string t = "Bono x2 activo: " + formatTime(game.boostLeft);
      drawTextCentered(t.c_str(), ad.x + ad.width / 2, ad.y + 38, 30, kInk);
    }
  }

  // ------------------------------------------------------------------ pantalla: mejoras
  static std::string upgradeDesc(int i) {
    const UpgradeDef& d = upgradeDef(i);
    switch (d.kind) {
      case UpgradeKind::ClickPower: return "+" + formatNumber(d.value) + " por clic";
      case UpgradeKind::CritChance: return "+" + std::to_string(static_cast<int>(std::round(d.value * 100))) + "% de clic x5";
      default: return "+" + formatNumber(d.value) + " por segundo";
    }
  }

  void drawPriceButton(Rectangle r, double cost, bool affordable, bool pressed) {
    const Rectangle face = drawButton(r, kGreen, affordable, pressed);
    const std::string t = formatNumber(cost);
    const int w = textWidth(t.c_str(), 30);
    const float total = 30.0f + 8.0f + static_cast<float>(w);
    const float x = face.x + (face.width - total) / 2;
    drawMiniSmiley({x + 14, face.y + face.height / 2}, 14, false, affordable ? 1.0f : 0.6f);
    drawText(t.c_str(), static_cast<int>(x + 38), static_cast<int>(face.y + face.height / 2 - 15), 30, WHITE);
  }

  void drawUpgrades(float dt) {
    const float contentH = kRowGap + kUpgradeCount * (kRowH + kRowGap) + 20;
    updateScroll(scrollUp, contentH, dt);
    for (int i = 0; i < kUpgradeCount; ++i) {
      const float y = L.contentTop + kRowGap + i * (kRowH + kRowGap) - scrollUp.y;
      if (y > L.contentBottom || y + kRowH < L.contentTop) continue;
      const Rectangle row = {24, y, kW - 48, kRowH};
      drawPanel(row, 26);

      const UpgradeDef& d = upgradeDef(i);
      const Color kindColor = d.kind == UpgradeKind::ClickPower ? kOrange : (d.kind == UpgradeKind::CritChance ? Color{150, 100, 255, 255} : kBlue);
      DrawCircleV({row.x + 58, row.y + kRowH / 2}, 38, kindColor);
      DrawCircleV({row.x + 58, row.y + kRowH / 2}, 31, withAlpha(WHITE, 0.25f));
      const std::string lv = std::to_string(game.upgradeLevel[i]);
      drawTextCentered(lv.c_str(), row.x + 58, row.y + kRowH / 2 - 15, 30, WHITE);

      drawText(d.name, static_cast<int>(row.x + 112), static_cast<int>(row.y + 20), 30, kInk);
      const std::string desc = upgradeDesc(i) + "  ·  nivel " + lv;
      drawText(desc.c_str(), static_cast<int>(row.x + 112), static_cast<int>(row.y + 62), 20, kInkSoft);

      const Rectangle btn = {row.x + row.width - 214, row.y + 18, 194, 76};
      if (game.upgradeMaxed(i)) {
        drawRound(btn, 20, {225, 232, 240, 255});
        drawTextCentered("MÁXIMO", btn.x + btn.width / 2, btn.y + 24, 30, kInkSoft);
      } else {
        const bool ok = game.canBuyUpgrade(i);
        drawPriceButton(btn, game.upgradeCost(i), ok, ok && holding(btn));
        if (tappedInView(btn)) {
          if (game.buyUpgrade(i)) {
            play(sfx.buy, 1.0f + 0.02f * game.upgradeLevel[i]);
            burst({btn.x + btn.width / 2, btn.y}, 5, -150, -30, 300, 560, 12, 18);
            saveNow();
          } else {
            play(sfx.error);
            say("Te faltan " + formatNumber(game.upgradeCost(i) - game.smiles) + " sonrisas");
          }
        }
      }
    }
  }

  // ------------------------------------------------------------------ pantalla: skins
  void drawSkins(float dt) {
    const int rows = (kSkinCount + 1) / 2;
    const float contentH = kRowGap + rows * (kCardH + 24) + 20;
    updateScroll(scrollSkin, contentH, dt);
    for (int i = 0; i < kSkinCount; ++i) {
      const int col = i % 2, row = i / 2;
      const float x = 24 + col * (kCardW + 24);
      const float y = L.contentTop + kRowGap + row * (kCardH + 24) - scrollSkin.y;
      if (y > L.contentBottom || y + kCardH < L.contentTop) continue;
      const Rectangle card = {x, y, kCardW, kCardH};
      const bool equipped = game.skinEquipped == i;
      drawPanel(card, 30);
      if (equipped) drawRoundLines(card, 30, 6, kGreen);

      drawSmiley({x + kCardW / 2, y + 140}, 66, i, 0.0f, 0.0f);
      const SkinDef& s = skinDef(i);
      drawTextCentered(s.name, x + kCardW / 2, y + 232, 30, kInk);
      drawTextCentered(s.tagline, x + kCardW / 2, y + 274, 20, kInkSoft);

      const Rectangle btn = {x + 24, y + kCardH - 70, kCardW - 48, 56};
      if (equipped) {
        drawRound(btn, 18, {214, 244, 224, 255});
        drawTextCentered("Equipada", btn.x + btn.width / 2, btn.y + 14, 30, {30, 130, 70, 255});
      } else if (game.skinOwned[i]) {
        const Rectangle face = drawButton(btn, kBlue, true, holding(btn));
        drawTextCentered("Usar", face.x + face.width / 2, face.y + 14, 30, WHITE);
        if (tappedInView(btn)) {
          game.equipSkin(i);
          play(sfx.buy, 1.2f);
          saveNow();
        }
      } else {
        const bool ok = game.smiles >= s.cost;
        drawPriceButton(btn, s.cost, ok, ok && holding(btn));
        if (tappedInView(btn)) {
          if (game.buySkin(i)) {
            play(sfx.win);
            burst({x + kCardW / 2, y + 140}, 14, -170, -10, 300, 700, 14, 24, true);
            say("¡Nueva skin: " + std::string(s.name) + "!");
            saveNow();
          } else {
            play(sfx.error);
            say("Te faltan " + formatNumber(s.cost - game.smiles) + " sonrisas");
          }
        }
      }
    }
  }

  // ------------------------------------------------------------------ pantalla: minijuegos
  void drawGames(float dt) {
    if (gmode == GamesMode::Menu) {
      const float contentH = 24 + 3 * (kGameCardH + 24) + 10;
      updateScroll(scrollGames, contentH, dt);
    }
    switch (gmode) {
      case GamesMode::Menu: drawGamesMenu(); break;
      case GamesMode::Catch: drawCatch(); break;
      case GamesMode::Wheel: drawWheel(); break;
      case GamesMode::Simon: drawSimon(); break;
    }
  }

  void drawGamesMenu() {
    const float h = kGameCardH;
    float y0 = L.contentTop + 24 - scrollGames.y;
    // Atrapa sonrisas
    {
      const Rectangle card = {24, y0, kW - 48, h};
      drawPanel(card, 30);
      drawText("Atrapa sonrisas", static_cast<int>(card.x + 28), static_cast<int>(card.y + 26), 40, kInk);
      drawText("Toca las sonrisas antes que", static_cast<int>(card.x + 28), static_cast<int>(card.y + 84), 30, kInkSoft);
      drawText("desaparezcan. Las doradas", static_cast<int>(card.x + 28), static_cast<int>(card.y + 122), 30, kInkSoft);
      drawText("valen 5. Dura 15 segundos.", static_cast<int>(card.x + 28), static_cast<int>(card.y + 160), 30, kInkSoft);
      drawMiniSmiley({card.x + card.width - 90, card.y + 70}, 40, false, 1.0f);
      drawMiniSmiley({card.x + card.width - 150, card.y + 130}, 28, false, 1.0f);
      drawMiniSmiley({card.x + card.width - 62, card.y + 150}, 24, true, 1.0f);
      const double cost = game.catchCost();
      const bool ok = game.smiles >= cost;
      const Rectangle btn = {card.x + 28, card.y + h - 100, card.width - 56, 76};
      drawPriceButton(btn, cost, ok, ok && holding(btn));
      if (tappedInView(btn)) {
        if (ok) {
          const float arenaH = L.contentBottom - (L.contentTop + 140) - 20;
          if (game.startCatch(arenaH / kArenaW)) {
            gmode = GamesMode::Catch;
            catchDoubled = false;
            play(sfx.buy);
            saveNow();
          }
        } else {
          play(sfx.error);
          say("Te faltan " + formatNumber(cost - game.smiles) + " sonrisas");
        }
      }
    }
    // Memoria de sonrisas
    {
      const Rectangle card = {24, y0 + h + 24, kW - 48, h};
      drawPanel(card, 30);
      drawText("Memoria de sonrisas", static_cast<int>(card.x + 28), static_cast<int>(card.y + 26), 40, kInk);
      drawText("Mira los colores y repite", static_cast<int>(card.x + 28), static_cast<int>(card.y + 84), 30, kInkSoft);
      drawText("la secuencia. Cada ronda", static_cast<int>(card.x + 28), static_cast<int>(card.y + 122), 30, kInkSoft);
      drawText("suma un color. Hasta 15.", static_cast<int>(card.x + 28), static_cast<int>(card.y + 160), 30, kInkSoft);
      if (game.simonBest > 0) {
        const std::string best = "Tu mejor: " + std::to_string(game.simonBest) + " rondas";
        drawText(best.c_str(), static_cast<int>(card.x + 28), static_cast<int>(card.y + 198), 20, kInkSoft);
      }
      const Color cols[4] = {kRed, kBlue, kGreen, kYellow};
      for (int i = 0; i < 4; ++i)
        drawRound({card.x + card.width - 130 + (i % 2) * 54.0f, card.y + 40 + (i / 2) * 54.0f, 48, 48}, 14, cols[i]);
      const double cost = game.simonCost();
      const bool ok = game.smiles >= cost;
      const Rectangle btn = {card.x + 28, card.y + h - 100, card.width - 56, 76};
      drawPriceButton(btn, cost, ok, ok && holding(btn));
      if (tappedInView(btn)) {
        if (ok) {
          if (game.startSimon()) {
            gmode = GamesMode::Simon;
            simonDoubled = false;
            lastLitPad = -1;
            play(sfx.buy);
            saveNow();
          }
        } else {
          play(sfx.error);
          say("Te faltan " + formatNumber(cost - game.smiles) + " sonrisas");
        }
      }
    }
    // Ruleta
    {
      const Rectangle card = {24, y0 + 2 * (h + 24), kW - 48, h};
      drawPanel(card, 30);
      drawText("Ruleta de la suerte", static_cast<int>(card.x + 28), static_cast<int>(card.y + 26), 40, kInk);
      drawText("Gira la ruleta y gana hasta", static_cast<int>(card.x + 28), static_cast<int>(card.y + 84), 30, kInkSoft);
      drawText("10 veces lo que apuestas.", static_cast<int>(card.x + 28), static_cast<int>(card.y + 122), 30, kInkSoft);
      drawText("Puedes ganar o perder.", static_cast<int>(card.x + 28), static_cast<int>(card.y + 160), 30, kInkSoft);
      drawWheelIcon({card.x + card.width - 96, card.y + 110}, 62);
      const Rectangle btn = {card.x + 28, card.y + h - 100, card.width - 56, 76};
      const Rectangle face = drawButton(btn, kBlue, true, holding(btn));
      drawTextCentered("Entrar", face.x + face.width / 2, face.y + 22, 30, WHITE);
      if (tappedInView(btn)) {
        gmode = GamesMode::Wheel;
        wheelResult.clear();
      }
    }
  }

  Rectangle arenaRect() const {
    const CatchState& c = game.catchGame;
    return {(kW - kArenaW) / 2, L.contentTop + 140, kArenaW, kArenaW * c.aspect};
  }

  void drawCatch() {
    const CatchState& c = game.catchGame;
    if (!c.active) {
      gmode = GamesMode::Menu;
      return;
    }
    const Rectangle arena = arenaRect();

    // Marcador
    const std::string left = formatTime(c.timeLeft);
    drawText(left.c_str(), 40, static_cast<int>(L.contentTop + 24), 60, kInk);
    const std::string pts = "Puntos: " + std::to_string(c.score);
    drawTextRight(pts.c_str(), kW - 40, L.contentTop + 38, 40, kInk);
    const float frac = std::clamp(c.timeLeft / c.duration, 0.0f, 1.0f);
    drawRound({40, L.contentTop + 100, kW - 80, 20}, 10, {255, 255, 255, 170});
    if (frac > 0) drawRound({40, L.contentTop + 100, (kW - 80) * frac, 20}, 10, frac > 0.25f ? kGreen : kRed);

    // Arena
    drawRound(arena, 28, {255, 255, 255, 90});
    drawRoundLines(arena, 28, 4, {255, 255, 255, 200});

    for (const CatchTarget& t : c.targets) {
      const float age = t.maxLife - t.life;
      const float pop = age < 0.15f ? 0.5f + 0.5f * (age / 0.15f) : 1.0f;
      const float a = std::min(1.0f, t.life / 0.3f);
      drawMiniSmiley({arena.x + t.x * arena.width, arena.y + t.y * arena.width}, t.radius * arena.width * pop, t.golden, a);
    }

    if (!c.finished) {
      if (!modal && ptr.pressed && inside(arena, ptr.pos)) {
        const float nx = (ptr.pos.x - arena.x) / arena.width;
        const float ny = (ptr.pos.y - arena.y) / arena.width;
        const int before = game.catchGame.score;
        if (game.tapCatch(nx, ny)) {
          const int gain = game.catchGame.score - before;
          burst(ptr.pos, 5, -180, 0, 200, 420, 10, 16, gain > 1);
          floatText({ptr.pos.x, ptr.pos.y - 30}, "+" + std::to_string(gain), gain > 1 ? kOrange : WHITE, 50);
          play(sfx.hit, gain > 1 ? 1.4f : frand(0.95f, 1.15f));
        }
      }
    } else {
      drawCatchResult(c);
    }
  }

  void drawCatchResult(const CatchState& c) {
    const Rectangle p = {70, L.contentTop + 190, kW - 140, 470};
    drawPanel(p, 34, {255, 255, 255, 250});
    drawTextCentered("¡Se acabó el tiempo!", kW / 2, p.y + 28, 40, kInk);
    const std::string a = "Atrapaste " + std::to_string(c.caught) + " sonrisas";
    drawTextCentered(a.c_str(), kW / 2, p.y + 96, 30, kInkSoft);
    if (c.goldenCaught > 0) {
      const std::string g = "(" + std::to_string(c.goldenCaught) + " doradas)";
      drawTextCentered(g.c_str(), kW / 2, p.y + 134, 30, {200, 130, 0, 255});
    }
    const std::string r = "+" + formatNumber(c.reward);
    drawTextCentered(r.c_str(), kW / 2, p.y + 186, 70, kGreen);
    drawTextCentered("sonrisas", kW / 2, p.y + 262, 30, kInkSoft);

    float by = p.y + 312;
    if (!catchDoubled && c.reward > 0) {
      const Rectangle dbl = {p.x + 30, by, p.width - 60, 70};
      const Rectangle face = drawButton(dbl, kOrange, true, holding(dbl));
      drawTextCentered(ads::isSimulated() ? "Duplicar (anuncio de prueba)" : "Duplicar con un anuncio", face.x + face.width / 2, face.y + 20, 30, WHITE);
      if (tapped(dbl)) requestAd(AdPurpose::DoubleCatch);
      by += 88;
    }
    const Rectangle ok = {p.x + 30, by, p.width - 60, 70};
    const Rectangle face = drawButton(ok, kGreen, true, holding(ok));
    drawTextCentered("Listo", face.x + face.width / 2, face.y + 20, 30, WHITE);
    if (tapped(ok)) {
      game.closeCatch();
      gmode = GamesMode::Menu;
      saveNow();
    }
  }

  // --- Memoria de sonrisas ---
  static Color padColor(int i) {
    static const Color cols[kSimonPads] = {kRed, kBlue, kGreen, kYellow};
    return cols[i % kSimonPads];
  }

  void drawSimon() {
    const SimonState& sm = game.simon;
    if (!sm.active) {
      gmode = GamesMode::Menu;
      return;
    }
    const float contentH = L.contentBottom - L.contentTop;
    const float side = std::clamp(contentH - 260.0f, 340.0f, 600.0f);
    const float pad = (side - 20.0f) / 2.0f;
    const Vector2 origin = {(kW - side) / 2, L.contentTop + 200.0f};

    // Encabezado
    const std::string round = "Ronda " + std::to_string(std::min(sm.rounds + 1, kSimonMaxRounds));
    drawText(round.c_str(), 40, static_cast<int>(L.contentTop + 24), 50, kInk);
    const char* status = sm.finished ? "" : (sm.phase == SimonState::Input ? "¡Tu turno!" : "Mira con atención...");
    drawTextRight(status, kW - 40, L.contentTop + 36, 40, sm.phase == SimonState::Input ? kGreen : kInkSoft);
    drawRound({40, L.contentTop + 100, kW - 80, 20}, 10, {255, 255, 255, 170});
    if (!sm.finished && sm.phase == SimonState::Input) {
      const float frac = std::clamp(sm.inputLeft / 6.0f, 0.0f, 1.0f);
      if (frac > 0) drawRound({40, L.contentTop + 100, (kW - 80) * frac, 20}, 10, frac > 0.3f ? kGreen : kRed);
    }
    const std::string prog = "Rondas hechas: " + std::to_string(sm.rounds) + " de " + std::to_string(kSimonMaxRounds);
    drawTextCentered(prog.c_str(), kW / 2, L.contentTop + 140, 30, kInkSoft);

    // Sonido al encenderse un color (lo muestra el juego o lo toca la persona)
    if (sm.litPad >= 0 && sm.litPad != lastLitPad) {
      static const float kPitch[kSimonPads] = {0.75f, 0.95f, 1.15f, 1.4f};
      play(sfx.hit, kPitch[sm.litPad]);
    }
    lastLitPad = sm.litPad;

    // Los cuatro colores
    int tappedPad = -1;
    for (int i = 0; i < kSimonPads; ++i) {
      const Rectangle r = {origin.x + (i % 2) * (pad + 20), origin.y + (i / 2) * (pad + 20), pad, pad};
      const bool lit = sm.litPad == i;
      const Color base = padColor(i);
      const Rectangle drawn = lit ? Rectangle{r.x - 6, r.y - 6, r.width + 12, r.height + 12} : r;
      drawRound({drawn.x, drawn.y + 8, drawn.width, drawn.height}, 40, darker(base, 0.55f));
      drawRound(drawn, 40, lit ? base : darker(base, 0.78f));
      if (lit) {
        drawRoundLines(drawn, 40, 6, WHITE);
        drawMiniSmiley({drawn.x + drawn.width / 2, drawn.y + drawn.height / 2}, pad * 0.26f, false, 1.0f);
      }
      if (!modal && !sm.finished && sm.phase == SimonState::Input && ptr.pressed && inside(r, ptr.pos)) tappedPad = i;
    }
    if (tappedPad >= 0) {
      const int before = game.simon.rounds;
      const bool good = game.tapSimon(tappedPad);
      if (!good) {
        play(sfx.error);
      } else if (game.simon.rounds > before) {
        burst({kW / 2, origin.y + side / 2}, 8, -170, -10, 300, 650, 12, 20, false);
      }
    }
    if (sm.finished) drawSimonResult(sm);
  }

  void drawSimonResult(const SimonState& sm) {
    const Rectangle p = {70, L.contentTop + 190, kW - 140, 470};
    drawPanel(p, 34, {255, 255, 255, 250});
    drawTextCentered(sm.won ? "¡Memoria perfecta!" : "¡Fin del juego!", kW / 2, p.y + 28, 40, kInk);
    const std::string a = "Pasaste " + std::to_string(sm.rounds) + (sm.rounds == 1 ? " ronda" : " rondas");
    drawTextCentered(a.c_str(), kW / 2, p.y + 96, 30, kInkSoft);
    const std::string r = "+" + formatNumber(sm.reward);
    drawTextCentered(r.c_str(), kW / 2, p.y + 186, 70, kGreen);
    drawTextCentered("sonrisas", kW / 2, p.y + 262, 30, kInkSoft);

    float by = p.y + 312;
    if (!simonDoubled && sm.reward > 0) {
      const Rectangle dbl = {p.x + 30, by, p.width - 60, 70};
      const Rectangle face = drawButton(dbl, kOrange, true, holding(dbl));
      drawTextCentered(ads::isSimulated() ? "Duplicar (anuncio de prueba)" : "Duplicar con un anuncio", face.x + face.width / 2, face.y + 20, 30, WHITE);
      if (tapped(dbl)) requestAd(AdPurpose::DoubleSimon);
      by += 88;
    }
    const Rectangle ok = {p.x + 30, by, p.width - 60, 70};
    const Rectangle face = drawButton(ok, kGreen, true, holding(ok));
    drawTextCentered("Listo", face.x + face.width / 2, face.y + 20, 30, WHITE);
    if (tapped(ok)) {
      game.closeSimon();
      gmode = GamesMode::Menu;
      saveNow();
    }
  }

  // ------------------------------------------------------------------ pantalla: metas
  void drawSectionTitle(const char* text, float y) {
    drawText(text, 28, static_cast<int>(y), 40, kInk);
  }

  void drawGoals(float dt) {
    constexpr float kDailyH = 360.0f, kMissionH = 136.0f, kMedalH = 98.0f, kTitleH = 70.0f;
    const float contentH = 16 + kTitleH + kDailyH + 12 + kTitleH + kMissionSlots * (kMissionH + 12) + kTitleH + 10 +
                           kAchievementCount * (kMedalH + 10) + 20;
    updateScroll(scrollGoals, contentH, dt);
    float y = L.contentTop + 16 - scrollGoals.y;

    // ---- Recompensa diaria
    drawSectionTitle("Recompensa diaria", y);
    y += kTitleH;
    {
      const Rectangle card = {24, y, kW - 48, kDailyH};
      drawPanel(card, 30);
      const bool avail = game.dailyAvailable(today);
      const int nextDay = game.dailyDay(today);
      const float chipW = 80, chipH = 128, gap = 10;
      const float x0 = card.x + (card.width - (kDailyDays * chipW + (kDailyDays - 1) * gap)) / 2;
      for (int d = 1; d <= kDailyDays; ++d) {
        const Rectangle chip = {x0 + (d - 1) * (chipW + gap), card.y + 24, chipW, chipH};
        const bool done = avail ? d < nextDay : d <= game.dailyStreak;
        const bool now = avail && d == nextDay;
        drawRound(chip, 18, done ? Color{214, 244, 224, 255} : (now ? Color{255, 240, 200, 255} : Color{238, 244, 250, 255}));
        if (now) drawRoundLines(chip, 18, 5, kOrange);
        const std::string dl = "Día " + std::to_string(d);
        drawTextCentered(dl.c_str(), chip.x + chipW / 2, chip.y + 12, 20, kInkSoft);
        if (done) {
          DrawCircleV({chip.x + chipW / 2, chip.y + 62}, 20, kGreen);
          DrawLineEx({chip.x + chipW / 2 - 9, chip.y + 62}, {chip.x + chipW / 2 - 2, chip.y + 70}, 5, WHITE);
          DrawLineEx({chip.x + chipW / 2 - 2, chip.y + 70}, {chip.x + chipW / 2 + 11, chip.y + 53}, 5, WHITE);
        } else {
          drawMiniSmiley({chip.x + chipW / 2, chip.y + 62}, 20, d == kDailyDays, 1.0f);
        }
        drawTextCentered(formatNumber(game.dailyAmount(d)).c_str(), chip.x + chipW / 2, chip.y + 96, 20, kInk);
      }
      if (avail) {
        const double amount = game.dailyAmount(nextDay);
        const Rectangle b1 = {card.x + 24, card.y + 180, card.width - 48, 72};
        const Rectangle f1 = drawButton(b1, kGreen, true, holding(b1));
        const std::string t1 = "Reclamar +" + formatNumber(amount);
        drawTextCentered(t1.c_str(), f1.x + f1.width / 2, f1.y + 20, 30, WHITE);
        if (tappedInView(b1)) {
          const double got = game.claimDaily(today, false);
          if (got > 0) {
            play(sfx.win);
            burst({b1.x + b1.width / 2, b1.y}, 12, -170, -10, 300, 700, 14, 22, true);
            say("¡Recompensa diaria! +" + formatNumber(got));
            saveNow();
          }
        }
        const Rectangle b2 = {card.x + 24, card.y + 266, card.width - 48, 72};
        const Rectangle f2 = drawButton(b2, kOrange, true, holding(b2));
        const std::string t2 = (ads::isSimulated() ? "x2 con anuncio de prueba  +" : "x2 con un anuncio  +") + formatNumber(amount * 2);
        drawTextCentered(t2.c_str(), f2.x + f2.width / 2, f2.y + 20, 30, WHITE);
        if (tappedInView(b2)) requestAd(AdPurpose::DoubleDaily);
      } else {
        drawTextCentered("¡Ya la reclamaste hoy!", card.x + card.width / 2, card.y + 190, 40, kGreen);
        const std::string t = "Mañana (día " + std::to_string(game.dailyDay(today + 1)) + "): +" +
                              formatNumber(game.dailyAmount(game.dailyDay(today + 1)));
        drawTextCentered(t.c_str(), card.x + card.width / 2, card.y + 256, 30, kInkSoft);
        drawTextCentered("Si no entras un día, la racha vuelve al día 1.", card.x + card.width / 2, card.y + 302, 20, kInkSoft);
      }
    }
    y += kDailyH + 12;

    // ---- Misiones
    drawSectionTitle("Misiones de hoy", y);
    y += kTitleH;
    for (int i = 0; i < kMissionSlots; ++i) {
      const Mission& m = game.missions[i];
      const Rectangle row = {24, y, kW - 48, kMissionH};
      y += kMissionH + 12;
      if (row.y > L.contentBottom || row.y + row.height < L.contentTop || m.target <= 0) continue;
      drawPanel(row, 26);
      const bool done = game.missionDone(i);
      drawText(missionTitle(m).c_str(), static_cast<int>(row.x + 24), static_cast<int>(row.y + 18), 30, kInk);
      const float barW = row.width - 24 - 210;
      const Rectangle bar = {row.x + 24, row.y + 78, barW, 22};
      drawRound(bar, 11, {225, 232, 240, 255});
      const float frac = static_cast<float>(game.missionProgress(i) / m.target);
      if (frac > 0) drawRound({bar.x, bar.y, std::max(22.0f, bar.width * frac), bar.height}, 11, done ? kGreen : kBlue);
      const std::string pr = formatNumber(std::floor(game.missionProgress(i))) + " / " + formatNumber(m.target);
      drawText(pr.c_str(), static_cast<int>(row.x + 24), static_cast<int>(row.y + 104), 20, kInkSoft);

      const Rectangle btn = {row.x + row.width - 180, row.y + 24, 156, 88};
      if (m.claimed) {
        drawRound(btn, 20, {214, 244, 224, 255});
        drawTextCentered("¡Hecho!", btn.x + btn.width / 2, btn.y + 28, 30, {30, 130, 70, 255});
      } else {
        const Rectangle face = drawButton(btn, kGreen, done, done && holding(btn));
        const std::string rw = "+" + formatNumber(m.reward);
        drawTextCentered(done ? "Reclamar" : "Premio", face.x + face.width / 2, face.y + 12, 20, WHITE);
        drawTextCentered(rw.c_str(), face.x + face.width / 2, face.y + 40, 30, WHITE);
        if (done && tappedInView(btn) && game.claimMission(i)) {
          play(sfx.win);
          burst({btn.x + btn.width / 2, btn.y}, 8, -160, -20, 300, 600, 12, 20, true);
          say("¡Misión cumplida! +" + formatNumber(m.reward));
          saveNow();
        }
      }
    }

    // ---- Medallas
    const std::string medalTitle = "Medallas " + std::to_string(game.achievementCount()) + "/" + std::to_string(kAchievementCount);
    drawSectionTitle(medalTitle.c_str(), y);
    const std::string bonus = "+" + std::to_string(static_cast<int>(std::round(kAchievementBonus * game.achievementCount() * 100))) + "% sonrisas";
    drawTextRight(bonus.c_str(), kW - 28, y + 12, 30, kGreen);
    y += kTitleH + 10;
    for (int i = 0; i < kAchievementCount; ++i) {
      const Rectangle row = {24, y, kW - 48, kMedalH};
      y += kMedalH + 10;
      if (row.y > L.contentBottom || row.y + row.height < L.contentTop) continue;
      const bool got = game.achievementUnlocked(i);
      const AchievementDef& d = achievementDef(i);
      drawPanel(row, 24, got ? Color{255, 255, 255, 240} : Color{255, 255, 255, 170});
      const Vector2 ic = {row.x + 56, row.y + kMedalH / 2};
      if (got) {
        drawMedalIcon(ic, 62);
      } else {
        DrawCircleV(ic, 32, {215, 222, 232, 255});
        drawLockIcon(ic, 40, {150, 160, 180, 255});
      }
      drawText(d.name, static_cast<int>(row.x + 112), static_cast<int>(row.y + 14), 30, got ? kInk : kInkSoft);
      drawText(d.desc, static_cast<int>(row.x + 112), static_cast<int>(row.y + 56), 20, kInkSoft);
      if (!got) {
        const double v = std::min(game.achievementValue(d.stat), d.threshold);
        const std::string pr = formatNumber(std::floor(v)) + "/" + formatNumber(d.threshold);
        drawTextRight(pr.c_str(), row.x + row.width - 22, row.y + 32, 20, kInkSoft);
      } else {
        drawTextRight("+2%", row.x + row.width - 22, row.y + 30, 30, kGreen);
      }
    }
  }

  // --- Ruleta ---
  static Color wheelColor(double mult) {
    if (mult == 0) return {170, 180, 195, 255};
    if (mult < 1) return {120, 190, 255, 255};
    if (mult == 1) return {90, 205, 130, 255};
    if (mult == 2) return {255, 214, 51, 255};
    if (mult == 3) return {255, 150, 70, 255};
    if (mult == 5) return {255, 92, 138, 255};
    return {160, 110, 255, 255};
  }

  static std::string multLabel(double m) {
    char buf[16];
    if (m == 0.5) return "x0.5";
    std::snprintf(buf, sizeof buf, "x%d", static_cast<int>(m));
    return buf;
  }

  void startWheelAnim(int slice) {
    // La casilla que queda bajo la flecha (arriba, a 270°) es: (270 - ángulo) mod 360.
    const float slot = 360.0f / kWheelSlices;
    const float centerLocal = slice * slot + slot / 2 + frand(-slot * 0.3f, slot * 0.3f);
    const float targetMod = std::fmod(270.0f - centerLocal + 720.0f, 360.0f);
    const float curMod = std::fmod(wheelAngle, 360.0f);
    const float delta = std::fmod(targetMod - curMod + 720.0f, 360.0f);
    wheelFrom = wheelAngle;
    wheelTo = wheelAngle + 360.0f * 5 + delta;
    wheelT = 0;
    wheelAnimating = true;
    wheelResult.clear();
  }

  void updateWheel(float dt) {
    if (!wheelAnimating) return;
    wheelT = std::min(1.0f, wheelT + dt / 4.2f);
    const float e = 1.0f - std::pow(1.0f - wheelT, 3.0f);
    wheelAngle = wheelFrom + (wheelTo - wheelFrom) * e;
    if (wheelT >= 1.0f) {
      wheelAnimating = false;
      const double win = game.wheel.winnings;
      const double bet = game.wheel.bet;
      game.claimWheel();
      saveNow();
      if (win > bet) {
        wheelResult = "¡Ganaste " + formatNumber(win) + "!";
        play(sfx.win);
        burst({kW / 2, L.contentTop + 350}, 16, -170, -10, 350, 800, 14, 24, true);
      } else if (win == bet) {
        wheelResult = "Recuperaste tu apuesta";
        play(sfx.buy);
      } else if (win > 0) {
        wheelResult = "Recuperaste " + formatNumber(win);
        play(sfx.error);
      } else {
        wheelResult = "Nada esta vez...";
        play(sfx.error);
      }
    }
  }

  void drawWheel() {
    const float contentH = L.contentBottom - L.contentTop;
    const float R = std::clamp((contentH - 400.0f) / 2.0f, 130.0f, 280.0f);
    const Vector2 c = {kW / 2, L.contentTop + 110 + R + 20};

    // Encabezado
    const Rectangle back = {24, L.contentTop + 16, 150, 62};
    const bool canLeave = !wheelAnimating;
    drawRound(back, 20, canLeave ? Color{255, 255, 255, 235} : Color{255, 255, 255, 120});
    drawTextCentered("< Volver", back.x + back.width / 2, back.y + 18, 30, canLeave ? kInk : kInkSoft);
    if (tapped(back) && canLeave) gmode = GamesMode::Menu;
    drawTextRight("Ruleta de la suerte", kW - 28, L.contentTop + 30, 30, kInk);

    // Rueda
    DrawCircleV(c, R + 16, kInk);
    const float slot = 360.0f / kWheelSlices;
    for (int i = 0; i < kWheelSlices; ++i) {
      const float a0 = wheelAngle + i * slot, a1 = a0 + slot;
      DrawCircleSector(c, R, a0, a1, 24, wheelColor(wheelMultiplier(i)));
    }
    for (int i = 0; i < kWheelSlices; ++i) {
      const float a = (wheelAngle + i * slot) * DEG2RAD;
      DrawLineEx(c, {c.x + std::cos(a) * R, c.y + std::sin(a) * R}, 3, {255, 255, 255, 200});
    }
    const Font& font = gfx::font();
    for (int i = 0; i < kWheelSlices; ++i) {
      const float mid = (wheelAngle + i * slot + slot / 2) * DEG2RAD;
      const std::string label = multLabel(wheelMultiplier(i));
      const float fs = R > 200 ? 40.0f : 30.0f;
      const Vector2 sz = MeasureTextEx(font, label.c_str(), fs, fs * 0.03f);
      const Vector2 pos = {c.x + std::cos(mid) * R * 0.66f, c.y + std::sin(mid) * R * 0.66f};
      float rot = mid * RAD2DEG;
      if (std::cos(mid) < 0) rot += 180.0f;  // en la mitad izquierda se gira para que el texto no quede al revés
      DrawTextPro(font, label.c_str(), pos, {sz.x / 2, sz.y / 2}, rot, fs, fs * 0.03f, kInk);
    }
    DrawRing(c, R - 4, R + 4, 0, 360, 64, kInk);
    DrawCircleV(c, R * 0.16f, kInk);
    DrawCircleV(c, R * 0.11f, WHITE);
    // Flecha
    DrawTriangle({c.x - 26, c.y - R - 34}, {c.x, c.y - R + 12}, {c.x + 26, c.y - R - 34}, kRed);
    DrawTriangle({c.x - 26, c.y - R - 34}, {c.x + 26, c.y - R - 34}, {c.x, c.y - R + 12}, kRed);

    // Resultado y botón
    const float ry = c.y + R + 40;
    if (!wheelResult.empty()) drawTextCentered(wheelResult.c_str(), kW / 2, ry, 50, kInk);
    else if (wheelAnimating) drawTextCentered("Girando...", kW / 2, ry, 40, kInkSoft);
    else drawTextCentered("Apuesta y gira", kW / 2, ry, 40, kInkSoft);

    const double cost = game.wheelCost();
    const bool ok = game.smiles >= cost && !wheelAnimating && !game.wheel.pending;
    const Rectangle btn = {110, L.contentBottom - 130, kW - 220, 84};
    const Rectangle face = drawButton(btn, kGreen, ok, ok && holding(btn));
    const std::string t = "Girar  " + formatNumber(cost);
    const int tw = textWidth(t.c_str(), 30);
    const float total = static_cast<float>(tw) + 40;
    const float x = face.x + (face.width - total) / 2;
    drawText(t.c_str(), static_cast<int>(x), static_cast<int>(face.y + 27), 30, WHITE);
    drawMiniSmiley({x + static_cast<float>(tw) + 24, face.y + face.height / 2}, 14, false, ok ? 1.0f : 0.6f);
    if (tapped(btn)) {
      if (ok && game.spinWheel()) {
        startWheelAnim(game.wheel.slice);
        play(sfx.buy, 0.8f);
      } else if (!wheelAnimating) {
        play(sfx.error);
        say("Te faltan " + formatNumber(cost - game.smiles) + " sonrisas");
      }
    }
  }

  // ------------------------------------------------------------------ capas de arriba
  void drawEffects() {
    for (const Particle& p : particles) {
      const float a = std::clamp(p.life / (p.maxLife * 0.35f), 0.0f, 1.0f);
      drawMiniSmiley(p.pos, p.size, p.golden, a);
    }
    for (const FloatText& t : texts) {
      const float a = std::clamp(t.life / (t.maxLife * 0.5f), 0.0f, 1.0f);
      drawOutlinedText(t.text.c_str(), t.pos.x, t.pos.y, t.size, withAlpha(t.color, a), withAlpha({60, 40, 20, 255}, a));
    }
  }

  void drawToast() {
    if (toastT <= 0) return;
    const int w = textWidth(toastText.c_str(), 30) + 56;
    const float a = std::clamp(toastT / 0.4f, 0.0f, 1.0f);
    const Rectangle r = {kW / 2 - w / 2.0f, L.contentBottom - 230, static_cast<float>(w), 64};
    drawRound(r, 32, withAlpha({43, 43, 58, 255}, 0.92f * a));
    drawText(toastText.c_str(), static_cast<int>(r.x + 28), static_cast<int>(r.y + 17), 30, withAlpha(WHITE, a));
  }

  void drawOfflinePopup() {
    DrawRectangle(0, 0, static_cast<int>(kW), static_cast<int>(L.H), {10, 30, 50, 150});
    const Rectangle p = {50, L.H / 2 - 270, kW - 100, 540};
    drawPanel(p, 36, WHITE);
    drawMiniSmiley({kW / 2, p.y + 76}, 46, false, 1.0f);
    drawTextCentered("¡Bienvenido de nuevo!", kW / 2, p.y + 146, 40, kInk);
    drawTextCentered("Mientras no estabas, tus amiguitos", kW / 2, p.y + 206, 30, kInkSoft);
    drawTextCentered("hicieron sonrisas por ti:", kW / 2, p.y + 244, 30, kInkSoft);
    const std::string g = "+" + formatNumber(offlineGain);
    drawTextCentered(g.c_str(), kW / 2, p.y + 292, 70, kGreen);

    const Rectangle dbl = {p.x + 30, p.y + 380, p.width - 60, 70};
    const Rectangle f1 = drawButton(dbl, kOrange, true, false);
    drawTextCentered(ads::isSimulated() ? "x2 (anuncio de prueba)" : "Duplicar con un anuncio", f1.x + f1.width / 2, f1.y + 20, 30, WHITE);
    const Rectangle ok = {p.x + 30, p.y + 470, p.width - 60, 60};
    drawRound(ok, 18, {225, 232, 240, 255});
    drawTextCentered("Recoger", ok.x + ok.width / 2, ok.y + 15, 30, kInk);
    if (tapped(dbl, true)) requestAd(AdPurpose::DoubleOffline);
    else if (tapped(ok, true)) {
      offlineOpen = false;
      play(sfx.buy);
    }
  }

  // Pantalla de prueba que sustituye al anuncio real cuando se juega en escritorio.
  void drawSimAd() {
    DrawRectangle(0, 0, static_cast<int>(kW), static_cast<int>(L.H), {10, 20, 40, 235});
    drawTextCentered("ANUNCIO DE PRUEBA", kW / 2, L.H / 2 - 170, 40, WHITE);
    drawTextCentered("Así se vería la pausa de un anuncio.", kW / 2, L.H / 2 - 100, 30, {200, 215, 235, 255});
    drawTextCentered("En Android aquí aparece un anuncio real de Google AdMob.", kW / 2, L.H / 2 - 62, 20, {170, 190, 215, 255});
    const Rectangle bar = {110, L.H / 2 + 20, kW - 220, 26};
    drawRound(bar, 13, {255, 255, 255, 60});
    drawRound({bar.x, bar.y, bar.width * ads::simProgress(), bar.height}, 13, kGreen);
    drawTextCentered("Cuando termine, recibes tu premio.", kW / 2, L.H / 2 + 80, 30, WHITE);
  }
};

}  // namespace

// ============================================================================================
// Modo de desarrollo: recorre las pantallas y guarda capturas (solo se compila con VERITY_DEV_SHOTS)
// ============================================================================================
#ifdef VERITY_DEV_SHOTS
namespace {

void shot(App& app, const std::string& dir, const char* name, int frames, bool keepEffects = false) {
  if (!keepEffects) {
    app.particles.clear();
    app.texts.clear();
  }
  for (int i = 0; i < frames; ++i) app.frame(1.0f / 60.0f);
  const std::string path = dir + "/" + name + ".png";
  Image img = LoadImageFromScreen();
  ExportImage(img, path.c_str());
  UnloadImage(img);
  std::printf("captura: %s\n", path.c_str());
}

void runShots(App& app, const std::string& dir, const std::string& prefix) {
  Game& g = app.game;
  // 1. Inicio
  app.tab = Tab::Play;
  shot(app, dir, (prefix + "01_inicio").c_str(), 6);

  // 2. Jugando con mejoras, bono y skin
  g.smiles = 12500;
  g.upgradeLevel[0] = 6;
  g.upgradeLevel[2] = 5;
  g.upgradeLevel[3] = 10;
  g.upgradeLevel[4] = 3;
  g.grantAdBoost();
  g.skinOwned[2] = true;
  g.equipSkin(2);
  for (int i = 0; i < 14; ++i) {
    const Vector2 bc = app.ballCenter();
    app.clickBall(bc, app.ballRadius(), {bc.x + (i % 5) * 20.0f - 40.0f, bc.y - 40});
    for (int k = 0; k < 3; ++k) app.frame(1.0f / 60.0f);
  }
  app.clickBall({kW / 2, app.L.contentTop + 400}, 190, {kW / 2, app.L.contentTop + 320});
  shot(app, dir, (prefix + "02_jugando").c_str(), 5, true);

  // 3. Mejoras
  app.tab = Tab::Upgrades;
  g.smiles = 9000;
  shot(app, dir, (prefix + "03_mejoras").c_str(), 4);
  app.scrollUp.y = 5000;
  shot(app, dir, (prefix + "04_mejoras_abajo").c_str(), 3);
  app.scrollUp.y = 0;

  // 4. Skins
  app.tab = Tab::Skins;
  g.smiles = 25000;
  g.skinOwned[1] = true;
  g.equipSkin(1);
  shot(app, dir, (prefix + "05_skins").c_str(), 4);
  app.scrollSkin.y = 5000;
  g.skinOwned[3] = false;
  shot(app, dir, (prefix + "06_skins_abajo").c_str(), 3);
  app.scrollSkin.y = 0;

  // 5. Minijuegos
  app.tab = Tab::Games;
  app.gmode = GamesMode::Menu;
  g.smiles = 5000;
  shot(app, dir, (prefix + "07_juegos").c_str(), 4);

  const float arenaH = app.L.contentBottom - (app.L.contentTop + 140) - 20;
  g.startCatch(arenaH / kArenaW);
  app.gmode = GamesMode::Catch;
  for (int i = 0; i < 60 * 5; ++i) {
    app.frame(1.0f / 60.0f);
    if (i % 70 == 0 && !g.catchGame.targets.empty()) {
      const Rectangle a = app.arenaRect();
      const CatchTarget& t = g.catchGame.targets.back();
      g.tapCatch(t.x, t.y);
      app.floatText({a.x + t.x * a.width, a.y + t.y * a.width}, "+1", WHITE, 50);
    }
  }
  shot(app, dir, (prefix + "08_atrapa").c_str(), 1, true);
  for (int i = 0; i < 60 * 12; ++i) app.frame(1.0f / 60.0f);
  shot(app, dir, (prefix + "09_atrapa_fin").c_str(), 2);
  g.closeCatch();

  // 6. Ruleta
  app.gmode = GamesMode::Wheel;
  g.smiles = 3000;
  shot(app, dir, (prefix + "10_ruleta").c_str(), 4);
  g.spinWheel();
  const int slice = g.wheel.slice;
  const double winnings = g.wheel.winnings;
  app.startWheelAnim(slice);
  shot(app, dir, (prefix + "11_ruleta_girando").c_str(), 60 * 2);
  shot(app, dir, (prefix + "12_ruleta_fin").c_str(), 60 * 3);
  std::printf("ruleta: casilla %d (%s) premio %.0f -> mensaje: %s\n", slice,
              App::multLabel(wheelMultiplier(slice)).c_str(), winnings, app.wheelResult.c_str());

  // 6b. Memoria de sonrisas
  app.gmode = GamesMode::Menu;
  g.smiles = 100000;
  app.tab = Tab::Games;
  shot(app, dir, (prefix + "15_juegos_menu").c_str(), 4);
  app.scrollGames.y = 5000;
  shot(app, dir, (prefix + "16_juegos_menu_abajo").c_str(), 4);
  app.scrollGames.y = 0;
  g.startSimon();
  app.gmode = GamesMode::Simon;
  for (int i = 0; i < 60 * 2 && g.simon.phase != SimonState::Show; ++i) app.frame(1.0f / 60.0f);
  for (int i = 0; i < 600 && g.simon.litPad < 0; ++i) app.frame(1.0f / 60.0f);
  shot(app, dir, (prefix + "17_memoria_mirando").c_str(), 1);
  for (int round = 0; round < 3; ++round) {
    for (int i = 0; i < 60 * 20 && g.simon.phase != SimonState::Input; ++i) app.frame(1.0f / 60.0f);
    const std::vector<int> seq = g.simon.seq;
    for (int pad : seq) g.tapSimon(pad);
  }
  for (int i = 0; i < 60 * 20 && g.simon.phase != SimonState::Input; ++i) app.frame(1.0f / 60.0f);
  shot(app, dir, (prefix + "18_memoria_turno").c_str(), 2);
  g.tapSimon((g.simon.seq[0] + 1) % kSimonPads);
  shot(app, dir, (prefix + "19_memoria_fin").c_str(), 3);
  g.closeSimon();
  app.gmode = GamesMode::Menu;

  // 6c. Metas: recompensa diaria, misiones y medallas
  app.tab = Tab::Goals;
  g.smiles = 5000;
  g.clicks = 3500;
  g.crits = 60;
  g.upgradesBought = 12;
  g.totalSmiles = 2.5e6;
  g.gamesPlayed = 6;
  g.upgradeLevel[3] = 20;
  g.upgradeLevel[2] = 3;
  app.today = 20000;
  g.dailyLast = 19999;
  g.dailyStreak = 3;
  g.missionDay = 0;
  g.refreshMissions(20000);
  g.missions[0].target = 50;
  g.missions[0].base = static_cast<double>(g.clicks) - 60;
  g.missions[1].base = g.missionStat(g.missions[1].kind) - g.missions[1].target * 0.4;
  g.missions[2].claimed = true;
  g.checkAchievements();
  shot(app, dir, (prefix + "20_metas").c_str(), 4);
  app.scrollGoals.y = 520;
  shot(app, dir, (prefix + "21_metas_misiones").c_str(), 4);
  app.scrollGoals.y = 5000;
  shot(app, dir, (prefix + "22_metas_medallas").c_str(), 4);
  app.scrollGoals.y = 0;
  g.dailyLast = 20000;
  g.dailyStreak = 4;
  shot(app, dir, (prefix + "23_metas_ya_reclamada").c_str(), 3);

  // 7. Ventanas emergentes
  app.tab = Tab::Play;
  app.gmode = GamesMode::Menu;
  app.offlineGain = 45600;
  app.offlineOpen = true;
  shot(app, dir, (prefix + "13_bienvenida").c_str(), 3);
  app.offlineOpen = false;
  g.boostLeft = 0;
  ads::showRewarded();
  shot(app, dir, (prefix + "14_anuncio_prueba").c_str(), 3);
}

}  // namespace
#endif

int main() {
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
  int winW = 450, winH = 800;
#ifdef VERITY_DEV_SHOTS
  if (const char* sz = std::getenv("VERITY_SHOT_SIZE")) std::sscanf(sz, "%dx%d", &winW, &winH);
#endif
  InitWindow(winW, winH, "Verity");
  SetWindowMinSize(300, 450);
  SetTargetFPS(60);
  gfx::initFonts();
#if !defined(__ANDROID__) && !defined(VERITY_DEV_SHOTS)
  {  // En escritorio la ventana empieza vertical y cabe en cualquier pantalla.
    const int mon = GetCurrentMonitor();
    const int mh = GetMonitorHeight(mon), mw = GetMonitorWidth(mon);
    if (mh > 0 && mw > 0) {
      const int h = std::min(900, static_cast<int>(mh * 0.85f));
      const int w = h * 9 / 16;
      SetWindowSize(w, h);
      SetWindowPosition((mw - w) / 2, std::max(20, (mh - h) / 2));
    }
  }
#endif

  App app;
#ifdef VERITY_DEV_SHOTS
  if (const char* dir = std::getenv("VERITY_SHOT_DIR")) {
    // Modo capturas: no toca el archivo de guardado real.
    app.sfx.ready = false;
    app.savePath = std::string(dir) + "/dev.sav";
    ads::init();
    runShots(app, dir, std::getenv("VERITY_SHOT_PREFIX") ? std::getenv("VERITY_SHOT_PREFIX") : "");
    ads::shutdown();
    gfx::shutdownFonts();
    CloseWindow();
    return 0;
  }
#endif
  app.init();
  while (!WindowShouldClose()) app.frame(GetFrameTime());
  app.shutdown();
  gfx::shutdownFonts();
  CloseWindow();
  return 0;
}
