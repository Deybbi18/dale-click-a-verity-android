// Pruebas del núcleo de Verity. No necesitan ventana ni raylib.
#include <cmath>
#include <cstdio>
#include <string>

#include "../src/game.hpp"

using namespace verity;

static int g_failed = 0;
static int g_total = 0;

#define CHECK(cond)                                                          \
  do {                                                                       \
    ++g_total;                                                               \
    if (!(cond)) {                                                           \
      ++g_failed;                                                            \
      std::printf("FALLA  linea %d: %s\n", __LINE__, #cond);                 \
    }                                                                        \
  } while (0)

#define CHECK_NEAR(a, b, eps)                                                \
  do {                                                                       \
    ++g_total;                                                               \
    if (!(std::fabs((a) - (b)) <= (eps))) {                                  \
      ++g_failed;                                                            \
      std::printf("FALLA  linea %d: %s = %g, esperado %g\n", __LINE__, #a,   \
                  (double)(a), (double)(b));                                 \
    }                                                                        \
  } while (0)

static void testFormat() {
  CHECK(formatNumber(0) == "0");
  CHECK(formatNumber(999) == "999");
  CHECK(formatNumber(999.9) == "999");
  CHECK(formatNumber(1000) == "1.00K");
  CHECK(formatNumber(1234) == "1.23K");
  CHECK(formatNumber(15300) == "15.3K");
  CHECK(formatNumber(999999) == "1.00M");  // sube de nivel en vez de mostrar 1000K
  CHECK(formatNumber(5600000) == "5.60M");
  CHECK(formatNumber(2.5e9) == "2.50B");
  CHECK(formatNumber(-1500) == "-1.50K");
  CHECK(formatTime(0) == "0:00");
  CHECK(formatTime(59.2) == "1:00");
  CHECK(formatTime(180) == "3:00");
  CHECK(formatTime(125) == "2:05");
}

static void testClicksAndUpgrades() {
  Game g(1);
  CHECK(g.perClick() == 1);
  CHECK(g.perSecond() == 0);
  CHECK(g.skinOwned[0] && g.skinEquipped == 0);

  ClickResult r = g.click();
  CHECK(r.gained == 1 && !r.crit);
  CHECK(g.smiles == 1 && g.clicks == 1);

  // Primer nivel de "Dedo fuerte" cuesta 15; el siguiente ceil(15*1.15)=18.
  CHECK(g.upgradeCost(0) == 15);
  CHECK(!g.buyUpgrade(0));        // no alcanza
  CHECK(g.smiles == 1);           // y no cobra nada
  g.smiles = 100;
  CHECK(g.buyUpgrade(0));
  CHECK(g.smiles == 85);
  CHECK(g.upgradeCost(0) == 18);
  CHECK(g.perClick() == 2);

  // Pasivas
  g.smiles = 1e6;
  CHECK(g.buyUpgrade(3));  // Amiguito: +1/s
  CHECK(g.buyUpgrade(4));  // Fábrica: +8/s
  CHECK(g.perSecond() == 9);
  const double before = g.smiles;
  const double totalBefore = g.totalSmiles;
  g.tick(2.0);
  CHECK_NEAR(g.smiles - before, 18.0, 1e-9);
  CHECK_NEAR(g.totalSmiles - totalBefore, 18.0, 1e-9);  // el total acumulado sube igual
  g.tick(1000.0);  // una pausa larga no regala sonrisas: cada paso se limita a 5 s
  CHECK_NEAR(g.smiles - before, 18.0 + 45.0, 1e-9);
  g.tick(-3.0);    // tiempo negativo se ignora
  CHECK_NEAR(g.smiles - before, 18.0 + 45.0, 1e-9);

  // Manos de oro
  CHECK(g.buyUpgrade(1));
  CHECK(g.perClick() == 12);

  // Suerte dorada llega al máximo de 25 niveles y no pasa de 50 %
  g.smiles = 1e18;
  int bought = 0;
  while (g.buyUpgrade(2)) ++bought;
  CHECK(bought == 25);
  CHECK(g.upgradeMaxed(2));
  CHECK_NEAR(g.critChance(), 0.5, 1e-9);
  CHECK(!g.canBuyUpgrade(2));

  // Índices inválidos no rompen nada
  CHECK(!g.buyUpgrade(-1));
  CHECK(!g.buyUpgrade(kUpgradeCount));
}

static void testCrits() {
  Game g(7);
  g.upgradeLevel[2] = 25;  // 50 %
  int crits = 0;
  const int n = 20000;
  for (int i = 0; i < n; ++i) {
    ClickResult r = g.click();
    if (r.crit) {
      ++crits;
      CHECK(r.gained == kCritMultiplier * 1.0);
    }
  }
  const double rate = static_cast<double>(crits) / n;
  CHECK(rate > 0.47 && rate < 0.53);

  Game none(7);
  for (int i = 0; i < 1000; ++i) CHECK(!none.click().crit);
}

static void testBoost() {
  Game g(2);
  g.upgradeLevel[0] = 4;  // 5 por clic
  g.upgradeLevel[3] = 10; // 10 por segundo
  CHECK(g.perClick() == 5 && g.perSecond() == 10);
  CHECK(g.canWatchAd());
  g.grantAdBoost();
  CHECK(g.multiplier() == 2);
  CHECK(g.perClick() == 10 && g.perSecond() == 20);
  CHECK(!g.canWatchAd());  // todavía queda mucho bono
  for (int i = 0; i < static_cast<int>(kBoostSeconds) - 30; ++i) g.tick(1.0);
  CHECK(g.boostLeft > 29 && g.boostLeft < 31);
  CHECK(g.canWatchAd());   // ya se puede renovar
  for (int i = 0; i < 40; ++i) g.tick(1.0);
  CHECK(g.boostLeft == 0);
  CHECK(g.multiplier() == 1 && g.perClick() == 5);
  g.grantBonus(500);
  CHECK(g.smiles >= 500);
  g.grantBonus(-5);  // valores raros se ignoran
  g.grantBonus(0);
}

static void testSkins() {
  Game g(3);
  CHECK(!g.buySkin(0));   // la clásica ya es tuya
  CHECK(!g.buySkin(1));   // sin dinero
  CHECK(!g.skinOwned[1]);
  CHECK(!g.equipSkin(1)); // sin comprarla no se puede usar
  g.smiles = 10000;
  CHECK(g.buySkin(1));
  CHECK(g.smiles == 9000);
  CHECK(g.skinOwned[1] && g.skinEquipped == 1);
  CHECK(!g.buySkin(1));   // no se cobra dos veces
  CHECK(g.smiles == 9000);
  CHECK(g.equipSkin(0));
  CHECK(g.skinEquipped == 0);
  CHECK(g.equipSkin(1));
  CHECK(!g.equipSkin(9));
  CHECK(!g.buySkin(-1));
}

static void testCatchGame() {
  Game g(11);
  g.smiles = 100;
  CHECK(!g.startCatch(1.2f));  // 250 es el mínimo
  CHECK(g.smiles == 100);
  g.smiles = 1000;
  CHECK(g.catchCost() == 250);
  CHECK(g.startCatch(1.2f));
  CHECK(g.smiles == 750);
  CHECK(g.catchGame.active && !g.catchGame.finished);
  CHECK(!g.startCatch(1.2f));  // ya hay una partida

  // Simula 15 segundos tocando siempre el objetivo más reciente
  int taps = 0;
  double reward = 0;
  for (int frame = 0; frame < 15 * 60 + 10 && !g.catchGame.finished; ++frame) {
    g.updateCatch(1.0 / 60.0);
    if (frame % 20 == 0 && !g.catchGame.targets.empty()) {
      const CatchTarget& t = g.catchGame.targets.back();
      if (g.tapCatch(t.x, t.y)) ++taps;
    }
    for (const CatchTarget& t : g.catchGame.targets) {
      CHECK(t.x >= t.radius - 1e-4f && t.x <= 1.0f - t.radius + 1e-4f);
      CHECK(t.y >= t.radius - 1e-4f && t.y <= 1.2f - t.radius + 1e-4f);
    }
  }
  CHECK(g.catchGame.finished);
  CHECK(taps > 5);
  CHECK(g.catchGame.caught == taps);
  CHECK(g.catchGame.score >= taps);
  reward = g.catchGame.reward;
  CHECK(reward == std::floor(g.catchGame.score * (12 + 3 * 1 + 0)));
  CHECK_NEAR(g.smiles, 750 + reward, 1e-6);
  CHECK(!g.tapCatch(0.5f, 0.5f));  // terminado: ya no se puede tocar
  g.closeCatch();
  CHECK(!g.catchGame.active);

  // Tocar fuera de todo objetivo no cuenta
  g.smiles = 1000;
  CHECK(g.startCatch(1.0f));
  g.updateCatch(0.3);
  CHECK(!g.tapCatch(-5.0f, -5.0f));
  g.closeCatch();

  // El costo sube con la riqueza (5 %)
  g.smiles = 1e6;
  CHECK(g.catchCost() == 50000);
}

static void testWheel() {
  Game g(5);
  g.smiles = 50;
  CHECK(!g.spinWheel());  // mínimo 100
  CHECK(g.smiles == 50);
  g.smiles = 1000;
  CHECK(g.wheelCost() == 100);
  CHECK(g.spinWheel());
  CHECK(g.smiles == 900);
  CHECK(g.wheel.pending);
  CHECK(!g.spinWheel());  // no se puede girar con un premio sin cobrar
  const double win = g.wheel.winnings;
  CHECK(win == std::floor(100 * wheelMultiplier(g.wheel.slice)));
  g.claimWheel();
  CHECK(!g.wheel.pending);
  CHECK_NEAR(g.smiles, 900 + win, 1e-9);
  g.claimWheel();  // cobrar dos veces no suma nada
  CHECK_NEAR(g.smiles, 900 + win, 1e-9);

  // Justicia: el valor esperado debe estar muy cerca de 1.0 (la casa casi no gana ni pierde)
  Game sim(99);
  double spent = 0, won = 0;
  int zero = 0, big = 0;
  const int n = 400000;
  for (int i = 0; i < n; ++i) {
    sim.smiles = 100000;
    sim.wheel = WheelState();
    const double cost = sim.wheelCost();
    sim.spinWheel();
    spent += cost;
    won += sim.wheel.winnings;
    if (sim.wheel.winnings == 0) ++zero;
    if (wheelMultiplier(sim.wheel.slice) >= 5) ++big;
  }
  const double ev = won / spent;
  std::printf("  ruleta: valor esperado simulado = %.3f (teorico 1.045), nada = %.1f%%, premios >= x5 = %.2f%%\n",
              ev, 100.0 * zero / n, 100.0 * big / n);
  CHECK(ev > 0.99 && ev < 1.10);
  CHECK(zero > n * 0.22 && zero < n * 0.26);
  CHECK(big > n * 0.02 && big < n * 0.04);
}

static void testSaveLoad() {
  Game a(1);
  a.smiles = 12345.5;
  a.totalSmiles = 99999;
  a.clicks = 4321;
  a.upgradeLevel[0] = 7;
  a.upgradeLevel[3] = 12;
  a.upgradeLevel[2] = 9;
  a.skinOwned[1] = a.skinOwned[3] = true;
  a.skinEquipped = 3;
  a.boostLeft = 100;
  a.soundOn = false;
  a.hintSeen = true;
  const int64_t t0 = 1700000000;
  const std::string data = a.serialize(t0);

  Game b(2);
  LoadResult r = b.deserialize(data, t0);  // sin tiempo fuera
  CHECK(r.ok);
  CHECK(r.offlineGain == 0);
  CHECK_NEAR(b.smiles, 12345.5, 1e-9);
  CHECK_NEAR(b.totalSmiles, 99999, 1e-9);
  CHECK(b.clicks == 4321);
  CHECK(b.upgradeLevel[0] == 7 && b.upgradeLevel[3] == 12 && b.upgradeLevel[2] == 9);
  CHECK(b.skinOwned[0] && b.skinOwned[1] && !b.skinOwned[2] && b.skinOwned[3]);
  CHECK(b.skinEquipped == 3);
  CHECK_NEAR(b.boostLeft, 100, 1e-9);
  CHECK(!b.soundOn && b.hintSeen);

  // Un hora fuera: 12 por segundo * 3600 s * 50 % = 21600
  Game c(3);
  r = c.deserialize(data, t0 + 3600);
  CHECK(r.ok);
  CHECK_NEAR(r.offlineGain, 21600, 1e-9);
  CHECK_NEAR(r.offlineSeconds, 3600, 1e-9);
  CHECK_NEAR(c.smiles, 12345.5 + 21600, 1e-6);
  CHECK(c.boostLeft == 0);  // el bono se gastó mientras estabas fuera

  // El tiempo fuera tiene tope de 2 horas
  Game d(4);
  r = d.deserialize(data, t0 + 10 * 3600);
  CHECK_NEAR(r.offlineSeconds, kOfflineCapSeconds, 1e-9);
  CHECK_NEAR(r.offlineGain, 12 * 7200 * 0.5, 1e-9);

  // Menos de un minuto no se premia
  Game e(5);
  r = e.deserialize(data, t0 + 30);
  CHECK(r.ok && r.offlineGain == 0);
  CHECK_NEAR(e.boostLeft, 70, 1e-9);

  // Un reloj que va hacia atrás no da ganancias negativas
  Game f(6);
  r = f.deserialize(data, t0 - 5000);
  CHECK(r.ok && r.offlineGain == 0);

  // Premio de ruleta sin cobrar se entrega al cargar
  Game w(8);
  w.smiles = 1000;
  w.spinWheel();
  w.wheel.winnings = 777;
  w.wheel.pending = true;
  const std::string midSpin = w.serialize(t0);
  Game w2(9);
  w2.deserialize(midSpin, t0);
  CHECK_NEAR(w2.smiles, 900 + 777, 1e-9);
  CHECK(!w2.wheel.pending);
}

static void testBadSaves() {
  Game g(1);
  g.smiles = 55;
  LoadResult r = g.deserialize("", 1000);
  CHECK(!r.ok);
  CHECK(g.smiles == 0);  // un guardado vacío deja el juego limpio

  r = g.deserialize("esto no es un guardado", 1000);
  CHECK(!r.ok);

  // Valores malos: se ignoran en vez de romper el juego
  const std::string evil =
      "verity_save 1\nsmiles -50\ntotal nan\nup0 99999999999\nup2 500\nup99 7\nskins 0\nequipped 4\n"
      "boost 99999\nsaved 100\n";
  r = g.deserialize(evil, 100);
  CHECK(r.ok);
  CHECK(g.smiles == 0 && g.totalSmiles == 0);
  CHECK(g.upgradeLevel[0] == 100000);   // tope razonable
  CHECK(g.upgradeLevel[2] == 25);       // respeta el máximo
  CHECK(g.skinOwned[0]);
  CHECK(g.skinEquipped == 0);           // equipada sin ser tuya: vuelve a la clásica
  CHECK(g.boostLeft <= kBoostSeconds);
}

static void testLongRunEconomy() {
  // Un jugador que compra lo más barato disponible debería poder ver todas las mejoras en pocos minutos.
  Game g(42);
  double t = 0;
  int firstTier[kUpgradeCount];
  for (int& v : firstTier) v = -1;
  while (t < 3600 * 2) {
    for (int i = 0; i < 6; ++i) g.click();  // ~6 clics por segundo
    g.tick(1.0);
    t += 1.0;
    bool bought = true;
    while (bought) {
      bought = false;
      int best = -1;
      double bestRatio = 0;
      for (int u = 0; u < kUpgradeCount; ++u) {
        if (!g.canBuyUpgrade(u)) continue;
        const UpgradeDef& d = upgradeDef(u);
        const double gain = d.kind == UpgradeKind::Passive ? d.value
                          : d.kind == UpgradeKind::ClickPower ? d.value * 6
                          : 1.0;
        const double ratio = gain / g.upgradeCost(u);
        if (ratio > bestRatio) { bestRatio = ratio; best = u; }
      }
      if (best >= 0) {
        g.buyUpgrade(best);
        if (firstTier[best] < 0) firstTier[best] = static_cast<int>(t);
        bought = true;
      }
    }
  }
  std::printf("  economia: primera compra de cada mejora (segundos): ");
  for (int u = 0; u < kUpgradeCount; ++u) std::printf("%d ", firstTier[u]);
  std::printf("\n  economia: tras 2 horas -> %s por segundo\n", formatNumber(g.perSecond()).c_str());
  CHECK(firstTier[0] >= 0 && firstTier[0] < 30);    // Dedo fuerte casi de inmediato
  CHECK(firstTier[3] >= 0 && firstTier[3] < 120);   // Amiguito en los primeros 2 minutos
  CHECK(firstTier[4] >= 0 && firstTier[4] < 900);   // Fábrica en menos de 15 minutos
  CHECK(firstTier[5] >= 0);                          // Arcoíris dentro de las 2 horas
  CHECK(g.perSecond() > 100);
}


static void testNewContent() {
  // Las tablas nuevas están completas y ordenadas por precio.
  CHECK(kUpgradeCount == 12 && kSkinCount == 5);
  for (int i = 0; i < kUpgradeCount; ++i) {
    CHECK(upgradeDef(i).name != nullptr && upgradeDef(i).baseCost > 0 && upgradeDef(i).growth > 1);
  }
  for (int i = 1; i < kSkinCount; ++i) CHECK(skinDef(i).cost > skinDef(i - 1).cost);

  Game g(3);
  g.smiles = 1e12;
  CHECK(g.buyUpgrade(8));   // Dedo de diamante +100
  CHECK(g.perClick() == 101);
  CHECK(g.buyUpgrade(10));  // Nube de risas +25000/s
  CHECK(g.perSecond() == 25000);
  CHECK(g.buySkin(4) && g.skinEquipped == 4);  // Corona
  CHECK(!g.buySkin(5));  // ya no hay más skins
}

static void testSimon() {
  Game g(21);
  g.smiles = 100;
  CHECK(!g.startSimon());  // mínimo 400
  CHECK(g.smiles == 100);
  g.smiles = 1000;
  CHECK(g.simonCost() == 400);
  CHECK(g.startSimon());
  CHECK(g.smiles == 600);
  CHECK(g.simon.active && g.simon.seq.size() == 1);
  CHECK(!g.startSimon());
  CHECK(!g.tapSimon(0));  // todavía está mostrando la secuencia

  // Juega 5 rondas perfectas "mirando" la secuencia como lo haría una persona.
  int rounds = 0;
  for (int guard = 0; guard < 100000 && rounds < 5; ++guard) {
    g.updateSimon(1.0 / 60.0);
    if (g.simon.phase == SimonState::Input) {
      const std::vector<int> seq = g.simon.seq;
      for (int pad : seq) CHECK(g.tapSimon(pad));
      ++rounds;
      CHECK(g.simon.rounds == rounds);
      CHECK(g.simon.seq.size() == static_cast<size_t>(rounds + 1));
    }
  }
  CHECK(g.simon.rounds == 5 && !g.simon.finished);
  // Fallar termina el juego y paga solo lo completado.
  while (g.simon.phase != SimonState::Input) g.updateSimon(1.0 / 60.0);
  const int wrong = (g.simon.seq[0] + 1) % kSimonPads;
  const double before = g.smiles;
  CHECK(!g.tapSimon(wrong));
  CHECK(g.simon.finished && !g.simon.won);
  CHECK(g.simon.reward == std::floor(5 * 2.5 * (12 + 3 * 1 + 0)));
  CHECK_NEAR(g.smiles, before + g.simon.reward, 1e-9);
  CHECK(g.simonBest == 5);
  CHECK(!g.tapSimon(0));
  g.closeSimon();
  CHECK(!g.simon.active);

  // Si no tocas nada, se acaba el tiempo.
  g.smiles = 1000;
  CHECK(g.startSimon());
  for (int i = 0; i < 60 * 30 && !g.simon.finished; ++i) g.updateSimon(1.0 / 60.0);
  CHECK(g.simon.finished && g.simon.reward == 0);
  g.closeSimon();

  // Ganar: llegar a la ronda máxima.
  g.smiles = 1e6;
  CHECK(g.startSimon());
  for (int guard = 0; guard < 2000000 && !g.simon.finished; ++guard) {
    g.updateSimon(1.0 / 60.0);
    if (g.simon.phase == SimonState::Input) {
      const std::vector<int> seq = g.simon.seq;
      for (int pad : seq) g.tapSimon(pad);
    }
  }
  CHECK(g.simon.finished && g.simon.won && g.simon.rounds == kSimonMaxRounds);
  CHECK(g.simonBest == kSimonMaxRounds);
  CHECK(g.checkAchievements().size() > 0);
}

static void testDaily() {
  Game g(1);
  const int64_t d0 = 20000;
  CHECK(g.dailyAvailable(d0));
  CHECK(g.dailyDay(d0) == 1);
  const double a1 = g.dailyAmount(1);
  CHECK(a1 == 200);
  CHECK(g.dailyAmount(7) == 1200);           // x6
  CHECK(g.claimDaily(d0, false) == 200);
  CHECK(g.smiles == 200);
  CHECK(!g.dailyAvailable(d0));
  CHECK(g.claimDaily(d0, false) == 0);       // una vez por día
  CHECK(g.smiles == 200);

  // Días seguidos suben la racha; con anuncio se duplica.
  CHECK(g.dailyAvailable(d0 + 1) && g.dailyDay(d0 + 1) == 2);
  CHECK(g.claimDaily(d0 + 1, true) == 600);  // 300 x 2
  CHECK(g.dailyStreak == 2);
  for (int d = 2; d < 7; ++d) CHECK(g.claimDaily(d0 + d, false) > 0);
  CHECK(g.dailyStreak == 7 && g.maxStreak == 7);
  CHECK(g.dailyDay(d0 + 7) == 1);            // da la vuelta
  CHECK(g.claimDaily(d0 + 7, false) == 200);

  // Si se pierde un día, la racha empieza otra vez.
  CHECK(g.dailyDay(d0 + 9) == 1);
  g.claimDaily(d0 + 9, false);
  CHECK(g.dailyStreak == 1);
  // Un reloj atrasado no regala premios.
  CHECK(!g.dailyAvailable(d0 + 3));
  // Escala con la producción del jugador.
  g.upgradeLevel[3] = 100;  // 100/s
  CHECK(g.dailyAmount(1) == 19000);   // 100*180 + 90 = 18090, redondeado hacia arriba a 2 cifras
}

static void testMissions() {
  Game g(5);
  g.upgradeLevel[2] = 1;  // con crítico disponible, puede salir esa misión
  g.refreshMissions(20000);
  CHECK(g.missionDay == 20000);
  Game g2(99);
  g2.upgradeLevel[2] = 1;
  g2.refreshMissions(20000);
  for (int i = 0; i < kMissionSlots; ++i) {
    CHECK(g.missions[i].kind == g2.missions[i].kind);       // mismo día, mismas misiones
    CHECK(g.missions[i].target == g2.missions[i].target);
    CHECK(g.missions[i].target > 0 && g.missions[i].reward >= 300);
    CHECK(!missionTitle(g.missions[i]).empty());
    for (int j = i + 1; j < kMissionSlots; ++j) CHECK(g.missions[i].kind != g.missions[j].kind);  // distintas
  }
  const Mission before = g.missions[0];
  g.refreshMissions(20000);  // mismo día: no cambia nada
  CHECK(g.missions[0].kind == before.kind && g.missions[0].base == before.base);

  // Sin "Suerte dorada" nunca aparece la misión de críticos.
  for (int day = 20000; day < 20100; ++day) {
    Game n(1);
    n.refreshMissions(day);
    for (const Mission& m : n.missions) CHECK(m.kind != static_cast<int>(MissionKind::Crits));
  }

  // Cumplir cada tipo de misión.
  Game p(7);
  p.smiles = 1e9;
  p.missions[0] = {static_cast<int>(MissionKind::Clicks), 10, 0, 500, false};
  p.missions[1] = {static_cast<int>(MissionKind::Upgrades), 2, 0, 700, false};
  p.missions[2] = {static_cast<int>(MissionKind::Games), 1, 0, 900, false};
  p.missionDay = 20000;
  CHECK(!p.missionDone(0) && !p.missionsClaimable());
  CHECK(!p.claimMission(0));
  for (int i = 0; i < 10; ++i) p.click();
  CHECK(p.missionDone(0) && p.missionProgress(0) == 10);
  p.buyUpgrade(0);
  CHECK(!p.missionDone(1));
  p.buyUpgrade(0);
  CHECK(p.missionDone(1));
  p.startCatch(1.0f);
  CHECK(p.missionDone(2));
  CHECK(p.missionsClaimable());
  const double s0 = p.smiles;
  CHECK(p.claimMission(0));
  CHECK_NEAR(p.smiles, s0 + 500, 1e-9);
  CHECK(!p.claimMission(0));  // no se cobra dos veces
  CHECK(p.claimMission(1) && p.claimMission(2));
  CHECK(!p.missionsClaimable());
  CHECK(p.missionProgress(0) == 10);  // el progreso no pasa de la meta
  for (int i = 0; i < 50; ++i) p.click();
  CHECK(p.missionProgress(0) == 10);

  // La misión de ganar sonrisas cuenta lo ganado desde el inicio del día.
  Game e(8);
  e.totalSmiles = 5000;
  e.missions[0] = {static_cast<int>(MissionKind::Earn), 1000, 5000, 400, false};
  e.missionDay = 1;
  CHECK(e.missionProgress(0) == 0);
  e.grantBonus(600);
  CHECK(!e.missionDone(0));
  e.grantBonus(600);
  CHECK(e.missionDone(0));
}

static void testAchievements() {
  Game g(1);
  CHECK(g.checkAchievements().empty());
  CHECK(g.achievementBonus() == 1.0);
  g.clicks = 100;
  std::vector<int> a = g.checkAchievements();
  CHECK(a.size() == 1 && a[0] == 0);
  CHECK(g.achievementUnlocked(0));
  CHECK(g.checkAchievements().empty());  // no se repite
  CHECK_NEAR(g.achievementBonus(), 1.02, 1e-12);
  CHECK_NEAR(g.perClick(), 1.02, 1e-12);
  g.upgradeLevel[3] = 10;
  CHECK_NEAR(g.perSecond(), 10.2, 1e-9);

  // Varias a la vez
  g.clicks = 20000;
  g.totalSmiles = 2e6;
  a = g.checkAchievements();
  CHECK(a.size() == 4);  // 1.000 y 10.000 clics, 1.000 y 1 millón de sonrisas
  CHECK(g.achievementCount() == 5);

  // Todas las medallas se pueden ganar: umbrales alcanzables y nombres completos.
  Game all(2);
  all.clicks = 1e6; all.totalSmiles = 1e13; all.upgradesBought = 1000; all.gamesPlayed = 100;
  all.crits = 1000; all.maxStreak = 7; all.simonBest = 15; all.goldenTotal = 100;
  for (int i = 0; i < kSkinCount; ++i) all.skinOwned[i] = true;
  CHECK(static_cast<int>(all.checkAchievements().size()) == kAchievementCount);
  CHECK_NEAR(all.achievementBonus(), 1.0 + kAchievementBonus * kAchievementCount, 1e-12);
  for (int i = 0; i < kAchievementCount; ++i) CHECK(achievementDef(i).name && achievementDef(i).desc);

  // Las medallas no cambian el reparto del minijuego de forma rara: sube igual que la producción.
  Game c(3);
  const double v0 = c.catchTargetValue();
  c.achMask = 0x7;
  CHECK(c.catchTargetValue() > v0);
}

static void testNewSave() {
  Game a(1);
  a.crits = 12; a.upgradesBought = 34; a.gamesPlayed = 5; a.caughtTotal = 66; a.goldenTotal = 7;
  a.simonBest = 6; a.dailyClaims = 3; a.maxStreak = 4; a.achMask = 0x5;
  a.dailyLast = 20123; a.dailyStreak = 4;
  a.upgradeLevel[11] = 3;
  a.skinOwned[4] = true; a.skinEquipped = 4;
  a.refreshMissions(20123);
  a.missions[1].claimed = true;
  const std::string data = a.serialize(1700000000);

  Game b(2);
  CHECK(b.deserialize(data, 1700000000).ok);
  CHECK(b.crits == 12 && b.upgradesBought == 34 && b.gamesPlayed == 5 && b.caughtTotal == 66 && b.goldenTotal == 7);
  CHECK(b.simonBest == 6 && b.dailyClaims == 3 && b.maxStreak == 4 && b.achMask == 0x5);
  CHECK(b.dailyLast == 20123 && b.dailyStreak == 4);
  CHECK(b.upgradeLevel[11] == 3 && b.skinOwned[4] && b.skinEquipped == 4);
  CHECK(b.missionDay == 20123);
  for (int i = 0; i < kMissionSlots; ++i) {
    CHECK(b.missions[i].kind == a.missions[i].kind && b.missions[i].target == a.missions[i].target);
    CHECK(b.missions[i].base == a.missions[i].base && b.missions[i].reward == a.missions[i].reward);
  }
  CHECK(b.missions[1].claimed && !b.missions[0].claimed);
  b.refreshMissions(20123);  // mismo día: se queda igual
  CHECK(b.missions[1].claimed);
  b.refreshMissions(20124);  // día nuevo: misiones nuevas sin cobrar
  CHECK(!b.missions[1].claimed && b.missionDay == 20124);

  // Un guardado viejo (sin nada de lo nuevo) sigue cargando bien.
  const std::string old = "verity_save 1\nsmiles 500\ntotal 900\nclicks 40\nup0 3\nskins 3\nequipped 1\nsaved 1000\n";
  Game o(3);
  CHECK(o.deserialize(old, 1000).ok);
  CHECK(o.smiles == 500 && o.upgradeLevel[0] == 3 && o.skinOwned[1]);
  CHECK(o.dailyAvailable(20000) && o.missions[0].target == 0);
  o.refreshMissions(20000);  // y puede crear las misiones nuevas
  CHECK(o.missions[0].target > 0);

  // Datos de misión corruptos no rompen nada
  Game m(4);
  const std::string bad = "verity_save 1\nmk0 99\nmt0 nan\nmk7 3\nmt0 -5\nmx1 2\ndstreak 900\nach -1\n";
  CHECK(m.deserialize(bad, 10).ok);
  CHECK(m.dailyStreak <= kDailyDays);
  m.refreshMissions(5);
  CHECK(m.missions[0].target > 0);
}

int main() {
  std::printf("Pruebas de Verity\n");
  testFormat();
  testClicksAndUpgrades();
  testCrits();
  testBoost();
  testSkins();
  testCatchGame();
  testWheel();
  testSaveLoad();
  testBadSaves();
  testLongRunEconomy();
  testNewContent();
  testSimon();
  testDaily();
  testMissions();
  testAchievements();
  testNewSave();
  std::printf("%d comprobaciones, %d fallaron\n", g_total, g_failed);
  return g_failed == 0 ? 0 : 1;
}
