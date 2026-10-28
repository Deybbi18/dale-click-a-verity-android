// Anuncios en escritorio: simulación para poder probar el juego. No muestra anuncios reales.
#include <chrono>

#include "ads.hpp"

namespace verity::ads {

namespace {
using Clock = std::chrono::steady_clock;
constexpr double kSimSeconds = 3.0;
bool g_showing = false;
bool g_reward = false;
Clock::time_point g_start;

double elapsed() { return std::chrono::duration<double>(Clock::now() - g_start).count(); }
}  // namespace

void init() {}
void shutdown() {}

bool rewardedReady() { return !g_showing; }

void showRewarded() {
  if (g_showing) return;
  g_showing = true;
  g_start = Clock::now();
}

bool pollReward() {
  if (g_showing && elapsed() >= kSimSeconds) {
    g_showing = false;
    g_reward = true;
  }
  const bool r = g_reward;
  g_reward = false;
  return r;
}

bool isShowing() { return g_showing; }
bool isSimulated() { return true; }

float simProgress() {
  if (!g_showing) return 0.0f;
  const double p = elapsed() / kSimSeconds;
  return static_cast<float>(p > 1.0 ? 1.0 : p);
}

void setBannerVisible(bool) {}
int bannerHeight() { return 0; }

}  // namespace verity::ads
