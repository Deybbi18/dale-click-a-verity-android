// Verity - núcleo del juego.
// Toda la lógica (economía, mejoras, skins, minijuegos, guardado) vive aquí.
// No depende de raylib, así que se puede probar sin ventana (ver tests/).
#pragma once

#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace verity {

// ---------------------------------------------------------------------------
// Datos del juego
// ---------------------------------------------------------------------------

enum class UpgradeKind {
  ClickPower,  // suma sonrisas por cada clic
  CritChance,  // suma probabilidad de clic crítico
  Passive      // suma sonrisas por segundo
};

struct UpgradeDef {
  const char* name;
  UpgradeKind kind;
  double value;     // sonrisas por clic, probabilidad (0.02 = 2 %) o sonrisas por segundo
  double baseCost;  // precio del primer nivel
  double growth;    // el precio se multiplica por este número en cada nivel
  int maxLevel;     // 0 = sin límite
};

struct SkinDef {
  const char* name;
  const char* tagline;
  double cost;
};

constexpr int kUpgradeCount = 12;
constexpr int kSkinCount = 5;
constexpr int kWheelSlices = 10;
constexpr int kSimonPads = 4;
constexpr int kSimonMaxRounds = 15;   // si llegas aquí, ganas el juego de memoria
constexpr int kMissionSlots = 3;      // misiones nuevas cada día
constexpr int kDailyDays = 7;         // la racha de recompensas diarias da la vuelta cada 7 días
constexpr double kAchievementBonus = 0.02;  // cada medalla suma +2 % a todas las sonrisas

constexpr double kCritMultiplier = 5.0;
constexpr double kBoostSeconds = 180.0;      // duración del bono x2 que da el anuncio
constexpr double kBoostRenewBelow = 60.0;    // se puede ver otro anuncio si quedan menos segundos
constexpr double kOfflineCapSeconds = 7200;  // máximo de tiempo fuera que se premia
constexpr double kOfflineRate = 0.5;         // fracción de lo que se gana estando fuera

// --- Misiones diarias, recompensa diaria y medallas ---
enum class MissionKind { Clicks, Earn, Upgrades, Games, Caught, Crits, Count };

enum class AchStat { Clicks, TotalSmiles, Upgrades, Skins, Games, Crits, Streak, SimonBest, Golden };

struct AchievementDef {
  const char* name;
  const char* desc;
  AchStat stat;
  double threshold;
};

constexpr int kAchievementCount = 19;

struct Mission {
  int kind = 0;         // MissionKind
  double target = 0;
  double base = 0;      // valor del contador al empezar el día
  double reward = 0;
  bool claimed = false;
};

const UpgradeDef& upgradeDef(int index);
const SkinDef& skinDef(int index);
const AchievementDef& achievementDef(int index);
double dailyMultiplier(int day);  // day = 1..kDailyDays
std::string missionTitle(const Mission& m);
double wheelMultiplier(int slice);

// Formatea números grandes: 1234 -> "1.23K", 5600000 -> "5.60M".
std::string formatNumber(double value);
// Formatea segundos como m:ss.
std::string formatTime(double seconds);

// ---------------------------------------------------------------------------
// Estados auxiliares
// ---------------------------------------------------------------------------

struct ClickResult {
  double gained = 0;
  bool crit = false;
};

// Minijuego "Atrapa sonrisas". La arena mide 1 de ancho y `aspect` de alto.
struct CatchTarget {
  float x = 0, y = 0;
  float radius = 0;
  float life = 0, maxLife = 0;
  float vx = 0, vy = 0;
  bool golden = false;
};

struct CatchState {
  bool active = false;    // la pantalla del minijuego está abierta
  bool finished = false;  // se acabó el tiempo y se muestra el resultado
  float duration = 15.0f;
  float timeLeft = 0;
  float aspect = 1.0f;
  float spawnTimer = 0;
  int score = 0;
  int caught = 0;
  int goldenCaught = 0;
  double reward = 0;
  std::vector<CatchTarget> targets;
};

// Ruleta de la suerte. El premio queda pendiente hasta que termina la animación.
struct WheelState {
  bool pending = false;
  int slice = 0;
  double bet = 0;
  double winnings = 0;
};

// Minijuego "Memoria de sonrisas": repite la secuencia de colores cada vez más larga.
struct SimonState {
  enum Phase { Intro, Show, Input };
  bool active = false;
  bool finished = false;
  bool won = false;
  Phase phase = Intro;
  std::vector<int> seq;
  int showIdx = 0;
  bool showOn = false;
  float timer = 0;
  int litPad = -1;
  float litTimer = 0;
  int inputIdx = 0;
  float inputLeft = 0;
  int rounds = 0;  // rondas completadas
  double reward = 0;
};

struct LoadResult {
  bool ok = false;
  double offlineGain = 0;     // sonrisas ganadas mientras el juego estaba cerrado
  double offlineSeconds = 0;  // tiempo que se premió
};

// ---------------------------------------------------------------------------
// El juego
// ---------------------------------------------------------------------------

class Game {
 public:
  explicit Game(uint32_t seed = 20240601u);

  // --- Estado guardado ---
  double smiles = 0;
  double totalSmiles = 0;
  uint64_t clicks = 0;
  int upgradeLevel[kUpgradeCount] = {};
  bool skinOwned[kSkinCount] = {};
  int skinEquipped = 0;
  double boostLeft = 0;
  bool soundOn = true;
  bool hintSeen = false;

  // Estadísticas de toda la vida (para misiones y medallas)
  uint64_t crits = 0;
  uint64_t upgradesBought = 0;
  uint64_t gamesPlayed = 0;
  uint64_t caughtTotal = 0;
  uint64_t goldenTotal = 0;
  int simonBest = 0;
  uint64_t dailyClaims = 0;
  int maxStreak = 0;
  uint32_t achMask = 0;  // medallas ya ganadas

  // Recompensa diaria (los días se cuentan como números enteros, ver platform::dayNumber)
  int64_t dailyLast = 0;
  int dailyStreak = 0;

  // Misiones de hoy
  int64_t missionDay = 0;
  Mission missions[kMissionSlots];

  // --- Estado temporal (no se guarda) ---
  CatchState catchGame;
  WheelState wheel;
  SimonState simon;

  void reset();

  // --- Economía ---
  double perClick() const;      // sonrisas por clic normal, con bono
  double perSecond() const;     // sonrisas por segundo, con bono
  double critChance() const;    // 0..0.5
  double multiplier() const;    // 2 con bono activo, 1 si no
  double achievementBonus() const;  // 1 + 2 % por cada medalla
  double totalMultiplier() const;   // bono x medallas
  ClickResult click();
  void tick(double dt);

  // --- Mejoras ---
  double upgradeCost(int i) const;
  bool upgradeMaxed(int i) const;
  bool canBuyUpgrade(int i) const;
  bool buyUpgrade(int i);

  // --- Skins ---
  bool buySkin(int i);
  bool equipSkin(int i);

  // --- Anuncio con recompensa ---
  bool canWatchAd() const;
  void grantAdBoost();                  // x2 durante kBoostSeconds
  void grantBonus(double amount);       // sonrisas extra (por ejemplo, duplicar lo ganado fuera)

  // --- Minijuego: atrapa sonrisas ---
  double catchCost() const;
  double catchTargetValue() const;
  bool startCatch(float arenaAspect);
  void updateCatch(double dt);
  bool tapCatch(float x, float y);
  void closeCatch();

  // --- Minijuego: ruleta ---
  double wheelCost() const;
  bool spinWheel();    // cobra la apuesta y decide el resultado
  void claimWheel();   // entrega el premio (al terminar la animación)

  // --- Minijuego: memoria de sonrisas ---
  double simonCost() const;
  bool startSimon();
  void updateSimon(double dt);
  bool tapSimon(int pad);   // true si acertó
  void closeSimon();

  // --- Recompensa diaria ---
  bool dailyAvailable(int64_t today) const;
  int dailyDay(int64_t today) const;           // qué día de la racha toca (1..7)
  double dailyAmount(int day) const;
  double claimDaily(int64_t today, bool doubled);  // devuelve lo entregado (0 si no estaba disponible)

  // --- Misiones ---
  void refreshMissions(int64_t today);          // crea las misiones si cambió el día
  double missionStat(int kind) const;
  double missionProgress(int i) const;          // 0..target
  bool missionDone(int i) const;
  bool claimMission(int i);
  bool missionsClaimable() const;

  // --- Medallas ---
  double achievementValue(AchStat stat) const;
  bool achievementUnlocked(int i) const;
  int achievementCount() const;
  std::vector<int> checkAchievements();         // devuelve las medallas recién ganadas

  // --- Guardado ---
  std::string serialize(int64_t nowUnix) const;
  LoadResult deserialize(const std::string& text, int64_t nowUnix);

  std::mt19937& rng() { return rng_; }

 private:
  void spawnTarget();
  void finishCatch();
  void finishSimon(bool won);
  void simonStartRound();
  double passiveBase() const;
  double clickBase() const;

  std::mt19937 rng_;
};

}  // namespace verity
