#include "platform.hpp"

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

#if defined(__ANDROID__)
#include <android_native_app_glue.h>
extern "C" struct android_app* GetAndroidApp(void);  // la define raylib
#endif

namespace fs = std::filesystem;

namespace verity::platform {

std::string savePath() {
#if defined(__ANDROID__)
  // Almacenamiento interno de la app: privado y no necesita permisos.
  android_app* app = GetAndroidApp();
  if (app && app->activity && app->activity->internalDataPath)
    return std::string(app->activity->internalDataPath) + "/verity.sav";
  return "verity.sav";
#else
  fs::path dir;
#if defined(_WIN32)
  if (const char* appdata = std::getenv("APPDATA")) dir = fs::path(appdata) / "Verity";
#else
  if (const char* xdg = std::getenv("XDG_DATA_HOME")) dir = fs::path(xdg) / "verity";
  else if (const char* home = std::getenv("HOME")) dir = fs::path(home) / ".local" / "share" / "verity";
#endif
  if (dir.empty()) return "verity.sav";
  std::error_code ec;
  fs::create_directories(dir, ec);
  if (ec) return "verity.sav";
  return (dir / "verity.sav").string();
#endif
}

std::string readFile(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return std::string();
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

bool writeFileAtomic(const std::string& path, const std::string& data) {
  const std::string tmp = path + ".tmp";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << data;
    out.flush();
    if (!out) return false;
  }
  std::error_code ec;
  fs::rename(tmp, path, ec);  // reemplaza el archivo anterior de una sola vez
  if (ec) {
    fs::remove(path, ec);
    fs::rename(tmp, path, ec);
  }
  return !ec;
}

int64_t unixNow() { return static_cast<int64_t>(std::time(nullptr)); }

int64_t dayNumber() {
  // Día local = (hora UTC + diferencia con la hora local) / 86400.
  const std::time_t t = std::time(nullptr);
  std::tm lt{}, gt{};
#if defined(_WIN32)
  localtime_s(&lt, &t);
  gmtime_s(&gt, &t);
#else
  localtime_r(&t, &lt);
  gmtime_r(&t, &gt);
#endif
  int dayDiff = (lt.tm_year - gt.tm_year) * 366 + (lt.tm_yday - gt.tm_yday);
  dayDiff = dayDiff > 1 ? -1 : (dayDiff < -1 ? 1 : dayDiff);  // cambio de año
  const int64_t offset = static_cast<int64_t>(dayDiff) * 86400 + (lt.tm_hour - gt.tm_hour) * 3600 +
                         (lt.tm_min - gt.tm_min) * 60 + (lt.tm_sec - gt.tm_sec);
  return (static_cast<int64_t>(t) + offset) / 86400;
}

float safeTop() {
#if defined(__ANDROID__)
  return 36.0f;  // deja espacio para la barra de estado o la muesca de la cámara
#else
  return 0.0f;
#endif
}

}  // namespace verity::platform
