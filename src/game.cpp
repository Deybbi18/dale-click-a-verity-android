#include "game.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <sstream>

namespace verity {

// ---------------------------------------------------------------------------
// Tablas
// ---------------------------------------------------------------------------

static const UpgradeDef kUpgrades[kUpgradeCount] = {
    {"Dedo fuerte", UpgradeKind::ClickPower, 1, 15, 1.15, 0},
    {"Manos de oro", UpgradeKind::ClickPower, 10, 1200, 1.17, 0},
    {"Suerte dorada", UpgradeKind::CritChance, 0.02, 150, 1.45, 25},
    {"Amiguito", UpgradeKind::Passive, 1, 50, 1.15, 0},
    {"Fábrica de sonrisas", UpgradeKind::Passive, 8, 600, 1.15, 0},
    {"Arcoíris", UpgradeKind::Passive, 55, 6000, 1.15, 0},
    {"Estrella feliz", UpgradeKind::Passive, 400, 70000, 1.15, 0},
    {"Galaxia sonriente", UpgradeKind::Passive, 3000, 800000, 1.15, 0},
    {"Dedo de diamante", UpgradeKind::ClickPower, 100, 25000, 1.17, 0},
    {"Toque cósmico", UpgradeKind::ClickPower, 1500, 2.5e6, 1.17, 0},
    {"Nube de risas", UpgradeKind::Passive, 25000, 1e7, 1.15, 0},
    {"Universo feliz", UpgradeKind::Passive, 250000, 1e8, 1.15, 0},
};

static const SkinDef kSkins[kSkinCount] = {
    {"Clásica", "La bolita de siempre", 0},
    {"Gafas de sol", "Con mucho estilo", 1000},
    {"Gorro de fiesta", "Hoy se celebra", 10000},
    {"Ojos de corazón", "Enamorada de las sonrisas", 100000},
    {"Corona", "La reina de las sonrisas", 1000000},
};

static const AchievementDef kAchievements[kAchievementCount] = {
    {"Primeros toques", "Toca la bolita 100 veces", AchStat::Clicks, 100},
    {"Dedo ligero", "Toca la bolita 1.000 veces", AchStat::Clicks, 1000},
    {"Dedo imparable", "Toca la bolita 10.000 veces", AchStat::Clicks, 10000},
    {"Mil sonrisas", "Junta 1.000 sonrisas en total", AchStat::TotalSmiles, 1e3},
    {"Millonario feliz", "Junta 1 millón de sonrisas", AchStat::TotalSmiles, 1e6},
    {"Mil millones", "Junta mil millones de sonrisas", AchStat::TotalSmiles, 1e9},
    {"Billonario", "Junta 1 billón de sonrisas", AchStat::TotalSmiles, 1e12},
    {"Comprador", "Compra 10 mejoras", AchStat::Upgrades, 10},
    {"Gran inversor", "Compra 50 mejoras", AchStat::Upgrades, 50},
    {"Dueño de todo", "Compra 200 mejoras", AchStat::Upgrades, 200},
    {"Coleccionista", "Ten 3 skins", AchStat::Skins, 3},
    {"Fashionista", "Ten todas las skins", AchStat::Skins, kSkinCount},
    {"Jugón", "Juega 5 minijuegos", AchStat::Games, 5},
    {"Rey del juego", "Juega 25 minijuegos", AchStat::Games, 25},
    {"Suertudo", "Haz 50 clics críticos", AchStat::Crits, 50},
    {"Súper suerte", "Haz 500 clics críticos", AchStat::Crits, 500},
    {"Constante", "Llega al día 7 de recompensas", AchStat::Streak, 7},
    {"Buena memoria", "Pasa 8 rondas de Memoria", AchStat::SimonBest, 8},
    {"Cazador dorado", "Atrapa 20 sonrisas doradas", AchStat::Golden, 20},
};

static const double kDailyMult[kDailyDays] = {1, 1.5, 2, 2.5, 3, 4, 6};

// Ruleta: multiplicador de cada casilla y su peso. Valor esperado = 1.045 (muy parejo).
static const double kWheelMult[kWheelSlices] = {0, 0.5, 1, 2, 0, 0.5, 1, 3, 5, 10};
static const double kWheelWeight[kWheelSlices] = {12, 15, 11, 13, 12, 15, 11, 8, 2.5, 0.5};

const UpgradeDef& upgradeDef(int index) {
  return kUpgrades[std::clamp(index, 0, kUpgradeCount - 1)];
}
const SkinDef& skinDef(int index) { return kSkins[std::clamp(index, 0, kSkinCount - 1)]; }
const AchievementDef& achievementDef(int index) {
  return kAchievements[std::clamp(index, 0, kAchievementCount - 1)];
}
double dailyMultiplier(int day) { return kDailyMult[std::clamp(day, 1, kDailyDays) - 1]; }

double wheelMultiplier(int slice) { return kWheelMult[std::clamp(slice, 0, kWheelSlices - 1)]; }

// ---------------------------------------------------------------------------
// Formato de números
// ---------------------------------------------------------------------------

std::string formatNumber(double value) {
  if (!(value == value)) return "0";
  if (value < 0) return "-" + formatNumber(-value);
  char buf[48];
  if (value < 1000.0) {
    std::snprintf(buf, sizeof buf, "%.0f", std::floor(value));
    return buf;
  }
  static const char* const kSuffix[] = {"K", "M", "B", "T", "Qa", "Qi", "Sx", "Sp"};
  const int last = 7;
  int idx = -1;
  double s = value;
  while (s >= 1000.0 && idx < last) {
    s /= 1000.0;
    ++idx;
  }
  if (s >= 1000.0) {  // más grande que todos los sufijos
    std::snprintf(buf, sizeof buf, "%.2e", value);
    return buf;
  }
  if (s >= 999.5 && idx < last) {  // evita "1000K"
    s /= 1000.0;
    ++idx;
  }
  const char* fmt = s < 10 ? "%.2f" : (s < 100 ? "%.1f" : "%.0f");
  std::snprintf(buf, sizeof buf, fmt, s);
  return std::string(buf) + kSuffix[idx];
}

std::string formatTime(double seconds) {
  if (seconds < 0) seconds = 0;
  const int total = static_cast<int>(std::ceil(seconds));
  char buf[16];
  std::snprintf(buf, sizeof buf, "%d:%02d", total / 60, total % 60);
  return buf;
}

// ---------------------------------------------------------------------------
// Economía
// ---------------------------------------------------------------------------

Game::Game(uint32_t seed) : rng_(seed) { reset(); }

void Game::reset() {
  smiles = 0;
  totalSmiles = 0;
  clicks = 0;
  for (int& l : upgradeLevel) l = 0;
  for (bool& o : skinOwned) o = false;
  skinOwned[0] = true;
  skinEquipped = 0;
  boostLeft = 0;
  soundOn = true;
  hintSeen = false;
  crits = upgradesBought = gamesPlayed = caughtTotal = goldenTotal = dailyClaims = 0;
  simonBest = maxStreak = 0;
  achMask = 0;
  dailyLast = 0;
  dailyStreak = 0;
  missionDay = 0;
  for (Mission& m : missions) m = Mission();
  catchGame = CatchState();
  wheel = WheelState();
  simon = SimonState();
}

double Game::multiplier() const { return boostLeft > 0 ? 2.0 : 1.0; }

double Game::passiveBase() const {
  double sum = 0;
  for (int i = 0; i < kUpgradeCount; ++i)
    if (kUpgrades[i].kind == UpgradeKind::Passive) sum += kUpgrades[i].value * upgradeLevel[i];
  return sum;
}

double Game::clickBase() const {
  double base = 1;
  for (int i = 0; i < kUpgradeCount; ++i)
    if (kUpgrades[i].kind == UpgradeKind::ClickPower) base += kUpgrades[i].value * upgradeLevel[i];
  return base;
}

int Game::achievementCount() const {
  int n = 0;
  for (int i = 0; i < kAchievementCount; ++i)
    if ((achMask >> i) & 1u) ++n;
  return n;
}

double Game::achievementBonus() const { return 1.0 + kAchievementBonus * achievementCount(); }
double Game::totalMultiplier() const { return multiplier() * achievementBonus(); }

double Game::perClick() const { return clickBase() * totalMultiplier(); }

double Game::perSecond() const { return passiveBase() * totalMultiplier(); }

double Game::critChance() const {
  double c = 0;
  for (int i = 0; i < kUpgradeCount; ++i)
    if (kUpgrades[i].kind == UpgradeKind::CritChance) c += kUpgrades[i].value * upgradeLevel[i];
  return std::min(0.5, c);
}

ClickResult Game::click() {
  ClickResult r;
  std::uniform_real_distribution<double> roll(0.0, 1.0);
  r.crit = roll(rng_) < critChance();
  if (r.crit) ++crits;
  r.gained = perClick() * (r.crit ? kCritMultiplier : 1.0);
  smiles += r.gained;
  totalSmiles += r.gained;
  ++clicks;
  return r;
}

void Game::tick(double dt) {
  if (!(dt > 0)) return;
  dt = std::min(dt, 5.0);
  const double gain = perSecond() * dt;
  smiles += gain;
  totalSmiles += gain;
  boostLeft = std::max(0.0, boostLeft - dt);
}

// ---------------------------------------------------------------------------
// Mejoras y skins
// ---------------------------------------------------------------------------

double Game::upgradeCost(int i) const {
  const UpgradeDef& d = upgradeDef(i);
  return std::ceil(d.baseCost * std::pow(d.growth, upgradeLevel[std::clamp(i, 0, kUpgradeCount - 1)]));
}

bool Game::upgradeMaxed(int i) const {
  const UpgradeDef& d = upgradeDef(i);
  return d.maxLevel > 0 && upgradeLevel[std::clamp(i, 0, kUpgradeCount - 1)] >= d.maxLevel;
}

bool Game::canBuyUpgrade(int i) const {
  if (i < 0 || i >= kUpgradeCount) return false;
  return !upgradeMaxed(i) && smiles >= upgradeCost(i);
}

bool Game::buyUpgrade(int i) {
  if (!canBuyUpgrade(i)) return false;
  smiles -= upgradeCost(i);
  ++upgradeLevel[i];
  ++upgradesBought;
  return true;
}

bool Game::buySkin(int i) {
  if (i < 0 || i >= kSkinCount || skinOwned[i]) return false;
  if (smiles < kSkins[i].cost) return false;
  smiles -= kSkins[i].cost;
  skinOwned[i] = true;
  skinEquipped = i;  // la estrenas de una vez
  return true;
}

bool Game::equipSkin(int i) {
  if (i < 0 || i >= kSkinCount || !skinOwned[i]) return false;
  skinEquipped = i;
  return true;
}

// ---------------------------------------------------------------------------
// Anuncio con recompensa
// ---------------------------------------------------------------------------

bool Game::canWatchAd() const { return boostLeft < kBoostRenewBelow; }

void Game::grantAdBoost() { boostLeft = kBoostSeconds; }

void Game::grantBonus(double amount) {
  if (!(amount > 0)) return;
  smiles += amount;
  totalSmiles += amount;
}

// ---------------------------------------------------------------------------
// Minijuego: atrapa sonrisas
// ---------------------------------------------------------------------------

double Game::catchCost() const { return std::max(250.0, std::floor(smiles * 0.05)); }

double Game::catchTargetValue() const {
  return (12.0 + 3.0 * clickBase() + 2.0 * passiveBase()) * totalMultiplier();
}

bool Game::startCatch(float arenaAspect) {
  if (catchGame.active) return false;
  const double cost = catchCost();
  if (smiles < cost) return false;
  smiles -= cost;
  ++gamesPlayed;
  catchGame = CatchState();
  catchGame.active = true;
  catchGame.aspect = std::max(0.5f, arenaAspect);
  catchGame.timeLeft = catchGame.duration;
  catchGame.spawnTimer = 0.2f;
  return true;
}

void Game::spawnTarget() {
  std::uniform_real_distribution<float> unit(0.0f, 1.0f);
  CatchTarget t;
  t.golden = unit(rng_) < 0.12f;
  t.radius = t.golden ? 0.065f : 0.085f;
  t.maxLife = t.golden ? 1.1f : 1.5f;
  t.life = t.maxLife;
  t.x = t.radius + unit(rng_) * (1.0f - 2 * t.radius);
  t.y = t.radius + unit(rng_) * (catchGame.aspect - 2 * t.radius);
  t.vx = (unit(rng_) - 0.5f) * 0.14f;
  t.vy = (unit(rng_) - 0.5f) * 0.14f;
  catchGame.targets.push_back(t);
}

void Game::finishCatch() {
  catchGame.timeLeft = 0;
  catchGame.finished = true;
  catchGame.targets.clear();
  catchGame.reward = std::floor(catchGame.score * catchTargetValue());
  smiles += catchGame.reward;
  totalSmiles += catchGame.reward;
}

void Game::updateCatch(double dt) {
  CatchState& c = catchGame;
  if (!c.active || c.finished) return;
  const float step = static_cast<float>(std::min(dt, 0.1));

  c.timeLeft -= step;
  for (CatchTarget& t : c.targets) {
    t.life -= step;
    t.x += t.vx * step;
    t.y += t.vy * step;
    if (t.x < t.radius || t.x > 1.0f - t.radius) t.vx = -t.vx;
    if (t.y < t.radius || t.y > c.aspect - t.radius) t.vy = -t.vy;
    t.x = std::clamp(t.x, t.radius, 1.0f - t.radius);
    t.y = std::clamp(t.y, t.radius, c.aspect - t.radius);
  }
  c.targets.erase(std::remove_if(c.targets.begin(), c.targets.end(),
                                 [](const CatchTarget& t) { return t.life <= 0; }),
                  c.targets.end());

  if (c.timeLeft <= 0) {
    finishCatch();
    return;
  }

  c.spawnTimer -= step;
  const float progress = 1.0f - c.timeLeft / c.duration;
  const float interval = 0.65f - 0.25f * progress;
  while (c.spawnTimer <= 0) {
    if (c.targets.size() < 6) spawnTarget();
    c.spawnTimer += interval;
  }
}

bool Game::tapCatch(float x, float y) {
  CatchState& c = catchGame;
  if (!c.active || c.finished) return false;
  for (int i = static_cast<int>(c.targets.size()) - 1; i >= 0; --i) {
    const CatchTarget& t = c.targets[i];
    const float dx = x - t.x, dy = y - t.y;
    const float r = t.radius * 1.2f;  // un poco de margen para dedos
    if (dx * dx + dy * dy <= r * r) {
      c.score += t.golden ? 5 : 1;
      ++c.caught;
      ++caughtTotal;
      if (t.golden) {
        ++c.goldenCaught;
        ++goldenTotal;
      }
      c.targets.erase(c.targets.begin() + i);
      return true;
    }
  }
  return false;
}

void Game::closeCatch() { catchGame = CatchState(); }

// ---------------------------------------------------------------------------
// Minijuego: ruleta
// ---------------------------------------------------------------------------

double Game::wheelCost() const { return std::max(100.0, std::floor(smiles * 0.08)); }

bool Game::spinWheel() {
  if (wheel.pending) return false;
  const double cost = wheelCost();
  if (smiles < cost) return false;
  smiles -= cost;
  ++gamesPlayed;
  std::discrete_distribution<int> pick(std::begin(kWheelWeight), std::end(kWheelWeight));
  wheel.pending = true;
  wheel.slice = pick(rng_);
  wheel.bet = cost;
  wheel.winnings = std::floor(cost * kWheelMult[wheel.slice]);
  return true;
}

void Game::claimWheel() {
  if (!wheel.pending) return;
  smiles += wheel.winnings;
  totalSmiles += wheel.winnings;
  wheel.pending = false;
}

// ---------------------------------------------------------------------------
// Minijuego: memoria de sonrisas
// ---------------------------------------------------------------------------

double Game::simonCost() const { return std::max(400.0, std::floor(smiles * 0.05)); }

void Game::simonStartRound() {
  simon.phase = SimonState::Intro;
  simon.timer = 0.7f;
  simon.showIdx = 0;
  simon.showOn = false;
  simon.inputIdx = 0;
}

bool Game::startSimon() {
  if (simon.active) return false;
  const double cost = simonCost();
  if (smiles < cost) return false;
  smiles -= cost;
  ++gamesPlayed;
  simon = SimonState();
  simon.active = true;
  std::uniform_int_distribution<int> pad(0, kSimonPads - 1);
  simon.seq.push_back(pad(rng_));
  simonStartRound();
  return true;
}

void Game::finishSimon(bool won) {
  simon.finished = true;
  simon.won = won;
  simon.litPad = -1;
  simon.reward = std::floor(simon.rounds * 2.5 * catchTargetValue());
  smiles += simon.reward;
  totalSmiles += simon.reward;
  simonBest = std::max(simonBest, simon.rounds);
}

void Game::updateSimon(double dt) {
  SimonState& s = simon;
  if (!s.active || s.finished) return;
  const float step = static_cast<float>(std::min(dt, 0.1));
  if (s.litPad >= 0) {
    s.litTimer -= step;
    if (s.litTimer <= 0) s.litPad = -1;
  }
  const float shown = std::max(0.28f, 0.50f - 0.02f * static_cast<float>(s.seq.size()));
  switch (s.phase) {
    case SimonState::Intro:
      s.timer -= step;
      if (s.timer <= 0) {
        s.phase = SimonState::Show;
        s.showIdx = 0;
        s.showOn = false;
        s.timer = 0;
      }
      break;
    case SimonState::Show:
      s.timer -= step;
      if (s.timer <= 0) {
        if (s.showOn) {
          s.showOn = false;
          s.litPad = -1;
          ++s.showIdx;
          s.timer = 0.18f;
        } else if (s.showIdx >= static_cast<int>(s.seq.size())) {
          s.phase = SimonState::Input;
          s.inputIdx = 0;
          s.inputLeft = 6.0f;
        } else {
          s.litPad = s.seq[s.showIdx];
          s.litTimer = shown;
          s.showOn = true;
          s.timer = shown;
        }
      }
      break;
    case SimonState::Input:
      s.inputLeft -= step;
      if (s.inputLeft <= 0) finishSimon(false);
      break;
  }
}

bool Game::tapSimon(int pad) {
  SimonState& s = simon;
  if (!s.active || s.finished || s.phase != SimonState::Input) return false;
  if (pad < 0 || pad >= kSimonPads) return false;
  s.litPad = pad;
  s.litTimer = 0.22f;
  if (s.seq[s.inputIdx] != pad) {
    finishSimon(false);
    return false;
  }
  s.inputLeft = 6.0f;
  ++s.inputIdx;
  if (s.inputIdx >= static_cast<int>(s.seq.size())) {
    ++s.rounds;
    if (s.rounds >= kSimonMaxRounds) {
      finishSimon(true);
    } else {
      std::uniform_int_distribution<int> next(0, kSimonPads - 1);
      s.seq.push_back(next(rng_));
      simonStartRound();
    }
  }
  return true;
}

void Game::closeSimon() { simon = SimonState(); }

// ---------------------------------------------------------------------------
// Recompensa diaria
// ---------------------------------------------------------------------------

// Redondea hacia arriba a 2 cifras significativas (1234 -> 1300) para que los números se vean limpios.
static double niceRound(double v) {
  if (!(v > 0)) return 0;
  if (v < 100) return std::ceil(v);
  const double mag = std::pow(10.0, std::floor(std::log10(v)) - 1.0);
  return std::ceil(v / mag) * mag;
}

bool Game::dailyAvailable(int64_t today) const { return today > 0 && dailyLast < today; }

int Game::dailyDay(int64_t today) const {
  if (dailyLast != 0 && dailyLast == today - 1) return dailyStreak % kDailyDays + 1;
  return 1;  // primera vez, o se perdió un día: la racha empieza de nuevo
}

double Game::dailyAmount(int day) const {
  const double base = std::max(200.0, passiveBase() * 180.0 + clickBase() * 90.0);
  return niceRound(base * dailyMultiplier(day));
}

double Game::claimDaily(int64_t today, bool doubled) {
  if (!dailyAvailable(today)) return 0;
  const int day = dailyDay(today);
  const double amount = dailyAmount(day) * (doubled ? 2.0 : 1.0);
  dailyLast = today;
  dailyStreak = day;
  ++dailyClaims;
  maxStreak = std::max(maxStreak, day);
  grantBonus(amount);
  return amount;
}

// ---------------------------------------------------------------------------
// Misiones diarias
// ---------------------------------------------------------------------------

double Game::missionStat(int kind) const {
  switch (static_cast<MissionKind>(kind)) {
    case MissionKind::Clicks: return static_cast<double>(clicks);
    case MissionKind::Earn: return totalSmiles;
    case MissionKind::Upgrades: return static_cast<double>(upgradesBought);
    case MissionKind::Games: return static_cast<double>(gamesPlayed);
    case MissionKind::Caught: return static_cast<double>(caughtTotal);
    case MissionKind::Crits: return static_cast<double>(crits);
    default: return 0;
  }
}

std::string missionTitle(const Mission& m) {
  const std::string n = formatNumber(m.target);
  switch (static_cast<MissionKind>(m.kind)) {
    case MissionKind::Clicks: return "Toca la bolita " + n + " veces";
    case MissionKind::Earn: return "Gana " + n + " sonrisas";
    case MissionKind::Upgrades: return "Compra " + n + (m.target == 1 ? " mejora" : " mejoras");
    case MissionKind::Games: return "Juega " + n + (m.target == 1 ? " minijuego" : " minijuegos");
    case MissionKind::Caught: return "Atrapa " + n + " sonrisas jugando";
    case MissionKind::Crits: return "Haz " + n + " clics críticos";
    default: return "";
  }
}

void Game::refreshMissions(int64_t today) {
  if (today <= 0) return;
  bool valid = true;
  for (const Mission& m : missions)
    if (m.target <= 0) valid = false;
  if (missionDay == today && valid) return;
  missionDay = today;

  // Mismas misiones para el mismo día: el generador depende solo de la fecha.
  std::mt19937 gen(static_cast<uint32_t>(today * 2654435761ull));
  std::vector<int> kinds;
  for (int k = 0; k < static_cast<int>(MissionKind::Count); ++k) {
    if (k == static_cast<int>(MissionKind::Crits) && critChance() <= 0) continue;
    kinds.push_back(k);
  }
  for (int i = static_cast<int>(kinds.size()) - 1; i > 0; --i)
    std::swap(kinds[i], kinds[gen() % static_cast<uint32_t>(i + 1)]);

  static const double kTiers[static_cast<int>(MissionKind::Count)][3] = {
      {100, 250, 500}, {0, 0, 0}, {3, 5, 8}, {1, 2, 3}, {15, 25, 40}, {5, 10, 20}};

  for (int s = 0; s < kMissionSlots; ++s) {
    Mission m;
    m.kind = kinds[s % kinds.size()];
    const int tier = static_cast<int>(gen() % 3);
    if (m.kind == static_cast<int>(MissionKind::Earn))
      m.target = niceRound(std::max(500.0, passiveBase() * 900.0 + clickBase() * 300.0) * (1.0 + 0.5 * tier));
    else
      m.target = kTiers[m.kind][tier];
    m.base = missionStat(m.kind);
    m.reward = niceRound(std::max(300.0, passiveBase() * 300.0 + clickBase() * 150.0) * (1.0 + 0.25 * tier));
    m.claimed = false;
    missions[s] = m;
  }
}

double Game::missionProgress(int i) const {
  if (i < 0 || i >= kMissionSlots || missions[i].target <= 0) return 0;
  const Mission& m = missions[i];
  return std::clamp(missionStat(m.kind) - m.base, 0.0, m.target);
}

bool Game::missionDone(int i) const {
  return i >= 0 && i < kMissionSlots && missions[i].target > 0 && missionProgress(i) >= missions[i].target;
}

bool Game::claimMission(int i) {
  if (!missionDone(i) || missions[i].claimed) return false;
  missions[i].claimed = true;
  grantBonus(missions[i].reward);
  return true;
}

bool Game::missionsClaimable() const {
  for (int i = 0; i < kMissionSlots; ++i)
    if (missionDone(i) && !missions[i].claimed) return true;
  return false;
}

// ---------------------------------------------------------------------------
// Medallas
// ---------------------------------------------------------------------------

double Game::achievementValue(AchStat stat) const {
  switch (stat) {
    case AchStat::Clicks: return static_cast<double>(clicks);
    case AchStat::TotalSmiles: return totalSmiles;
    case AchStat::Upgrades: return static_cast<double>(upgradesBought);
    case AchStat::Skins: {
      int n = 0;
      for (bool o : skinOwned) n += o ? 1 : 0;
      return n;
    }
    case AchStat::Games: return static_cast<double>(gamesPlayed);
    case AchStat::Crits: return static_cast<double>(crits);
    case AchStat::Streak: return maxStreak;
    case AchStat::SimonBest: return simonBest;
    case AchStat::Golden: return static_cast<double>(goldenTotal);
  }
  return 0;
}

bool Game::achievementUnlocked(int i) const {
  return i >= 0 && i < kAchievementCount && ((achMask >> i) & 1u);
}

std::vector<int> Game::checkAchievements() {
  std::vector<int> fresh;
  for (int i = 0; i < kAchievementCount; ++i) {
    if (achievementUnlocked(i)) continue;
    if (achievementValue(kAchievements[i].stat) >= kAchievements[i].threshold) {
      achMask |= (1u << i);
      fresh.push_back(i);
    }
  }
  return fresh;
}

// ---------------------------------------------------------------------------
// Guardado
// ---------------------------------------------------------------------------

std::string Game::serialize(int64_t nowUnix) const {
  std::ostringstream o;
  o.precision(17);
  o << "verity_save 1\n";
  o << "smiles " << smiles << "\n";
  o << "total " << totalSmiles << "\n";
  o << "clicks " << static_cast<double>(clicks) << "\n";
  for (int i = 0; i < kUpgradeCount; ++i) o << "up" << i << " " << upgradeLevel[i] << "\n";
  int mask = 0;
  for (int i = 0; i < kSkinCount; ++i)
    if (skinOwned[i]) mask |= (1 << i);
  o << "skins " << mask << "\n";
  o << "equipped " << skinEquipped << "\n";
  o << "boost " << boostLeft << "\n";
  o << "sound " << (soundOn ? 1 : 0) << "\n";
  o << "hint " << (hintSeen ? 1 : 0) << "\n";
  o << "wheel " << (wheel.pending ? wheel.winnings : 0.0) << "\n";
  o << "crits " << static_cast<double>(crits) << "\n";
  o << "ubought " << static_cast<double>(upgradesBought) << "\n";
  o << "gplayed " << static_cast<double>(gamesPlayed) << "\n";
  o << "caught " << static_cast<double>(caughtTotal) << "\n";
  o << "golden " << static_cast<double>(goldenTotal) << "\n";
  o << "simonbest " << simonBest << "\n";
  o << "dclaims " << static_cast<double>(dailyClaims) << "\n";
  o << "maxstreak " << maxStreak << "\n";
  o << "ach " << static_cast<double>(achMask) << "\n";
  o << "dlast " << static_cast<double>(dailyLast) << "\n";
  o << "dstreak " << dailyStreak << "\n";
  o << "mday " << static_cast<double>(missionDay) << "\n";
  for (int i = 0; i < kMissionSlots; ++i) {
    const Mission& m = missions[i];
    o << "mk" << i << " " << m.kind << "\n";
    o << "mt" << i << " " << m.target << "\n";
    o << "mb" << i << " " << m.base << "\n";
    o << "mr" << i << " " << m.reward << "\n";
    o << "mc" << i << " " << (m.claimed ? 1 : 0) << "\n";
  }
  o << "saved " << static_cast<double>(nowUnix) << "\n";
  return o.str();
}

static bool goodNumber(double v) { return std::isfinite(v) && v >= 0; }

LoadResult Game::deserialize(const std::string& text, int64_t nowUnix) {
  LoadResult result;
  reset();

  std::istringstream in(text);
  std::string line;
  bool header = false;
  double savedAt = 0;
  double pendingWheel = 0;
  int mask = 1;

  // Una línea por dato ("clave valor"). Las líneas ilegibles se saltan sin afectar a las demás.
  while (std::getline(in, line)) {
    const size_t space = line.find(' ');
    if (space == std::string::npos || space == 0) continue;
    const std::string key = line.substr(0, space);
    const char* start = line.c_str() + space + 1;
    char* end = nullptr;
    const double value = std::strtod(start, &end);
    if (end == start || !std::isfinite(value)) continue;
    if (key == "verity_save") header = true;
    else if (key == "smiles" && goodNumber(value)) smiles = value;
    else if (key == "total" && goodNumber(value)) totalSmiles = value;
    else if (key == "clicks" && goodNumber(value)) clicks = static_cast<uint64_t>(value);
    else if (key == "skins") mask = static_cast<int>(value);
    else if (key == "equipped") skinEquipped = static_cast<int>(value);
    else if (key == "boost" && goodNumber(value)) boostLeft = std::min(value, kBoostSeconds);
    else if (key == "sound") soundOn = value != 0;
    else if (key == "hint") hintSeen = value != 0;
    else if (key == "wheel" && goodNumber(value)) pendingWheel = value;
    else if (key == "saved" && goodNumber(value)) savedAt = value;
    else if (key == "crits" && goodNumber(value)) crits = static_cast<uint64_t>(value);
    else if (key == "ubought" && goodNumber(value)) upgradesBought = static_cast<uint64_t>(value);
    else if (key == "gplayed" && goodNumber(value)) gamesPlayed = static_cast<uint64_t>(value);
    else if (key == "caught" && goodNumber(value)) caughtTotal = static_cast<uint64_t>(value);
    else if (key == "golden" && goodNumber(value)) goldenTotal = static_cast<uint64_t>(value);
    else if (key == "simonbest" && goodNumber(value)) simonBest = static_cast<int>(std::min(value, 1000.0));
    else if (key == "dclaims" && goodNumber(value)) dailyClaims = static_cast<uint64_t>(value);
    else if (key == "maxstreak" && goodNumber(value)) maxStreak = static_cast<int>(std::min(value, 1000.0));
    else if (key == "ach" && goodNumber(value)) achMask = static_cast<uint32_t>(static_cast<uint64_t>(std::min(value, 4294967295.0)));
    else if (key == "dlast" && goodNumber(value)) dailyLast = static_cast<int64_t>(value);
    else if (key == "dstreak" && goodNumber(value)) dailyStreak = std::clamp(static_cast<int>(std::min(value, 100.0)), 0, kDailyDays);
    else if (key == "mday" && goodNumber(value)) missionDay = static_cast<int64_t>(value);
    else if (key.size() == 3 && key[0] == 'm' && key[2] >= '0' && key[2] < '0' + kMissionSlots &&
             (key[1] == 'k' || key[1] == 't' || key[1] == 'b' || key[1] == 'r' || key[1] == 'c')) {
      Mission& m = missions[key[2] - '0'];
      if (!goodNumber(value)) continue;
      if (key[1] == 'k') m.kind = std::clamp(static_cast<int>(value), 0, static_cast<int>(MissionKind::Count) - 1);
      else if (key[1] == 't') m.target = value;
      else if (key[1] == 'b') m.base = value;
      else if (key[1] == 'r') m.reward = value;
      else m.claimed = value != 0;
    }
    else if (key.size() > 2 && key.compare(0, 2, "up") == 0) {
      const int idx = std::atoi(key.c_str() + 2);
      if (idx >= 0 && idx < kUpgradeCount && goodNumber(value)) {
        int lvl = static_cast<int>(std::min(value, 100000.0));
        if (kUpgrades[idx].maxLevel > 0) lvl = std::min(lvl, kUpgrades[idx].maxLevel);
        upgradeLevel[idx] = lvl;
      }
    }
  }
  if (!header) {
    reset();
    return result;
  }

  for (int i = 0; i < kSkinCount; ++i) skinOwned[i] = (mask >> i) & 1;
  skinOwned[0] = true;
  if (skinEquipped < 0 || skinEquipped >= kSkinCount || !skinOwned[skinEquipped]) skinEquipped = 0;

  // Premio de la ruleta que se quedó sin cobrar por cerrar el juego a medio giro.
  if (pendingWheel > 0) {
    smiles += pendingWheel;
    totalSmiles += pendingWheel;
  }

  // Ganancias de cuando el juego estaba cerrado.
  const double away = std::max(0.0, static_cast<double>(nowUnix) - savedAt);
  boostLeft = std::max(0.0, boostLeft - away);
  if (savedAt > 0 && away >= 60.0) {
    const double counted = std::min(away, kOfflineCapSeconds);
    const double gain = std::floor(passiveBase() * achievementBonus() * counted * kOfflineRate);
    if (gain > 0) {
      smiles += gain;
      totalSmiles += gain;
      result.offlineGain = gain;
      result.offlineSeconds = counted;
    }
  }

  result.ok = true;
  return result;
}

}  // namespace verity
