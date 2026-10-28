// Anuncios en Android: puente entre C++ y la clase Java MainActivity, que usa Google AdMob.
//
// Los anuncios de AdMob solo se pueden pedir desde Java/Kotlin, así que C++ llama a estos métodos
// públicos de MainActivity (ver android/app/src/main/java/com/verity/game/MainActivity.java):
//   void    showRewardedAd()
//   boolean isRewardedReady()
//   boolean consumeReward()
//   boolean isAdShowing()
//   void    setBannerVisible(boolean)
#include <android/log.h>
#include <android_native_app_glue.h>
#include <jni.h>

#include "ads.hpp"

extern "C" struct android_app* GetAndroidApp(void);  // la define raylib

#define LOG_TAG "Verity"
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

namespace verity::ads {

namespace {

jmethodID g_show = nullptr;
jmethodID g_ready = nullptr;
jmethodID g_consume = nullptr;
jmethodID g_showing = nullptr;
jmethodID g_banner = nullptr;
bool g_ok = false;

JNIEnv* env() {
  android_app* app = GetAndroidApp();
  if (!app || !app->activity || !app->activity->vm) return nullptr;
  JavaVM* vm = app->activity->vm;
  JNIEnv* e = nullptr;
  if (vm->GetEnv(reinterpret_cast<void**>(&e), JNI_VERSION_1_6) == JNI_EDETACHED) {
    if (vm->AttachCurrentThread(&e, nullptr) != JNI_OK) return nullptr;
  }
  return e;
}

jobject activity() {
  android_app* app = GetAndroidApp();
  return (app && app->activity) ? app->activity->clazz : nullptr;  // en realidad es la instancia
}

bool callBool(jmethodID m) {
  JNIEnv* e = env();
  jobject a = activity();
  if (!g_ok || !e || !a) return false;
  const bool r = e->CallBooleanMethod(a, m) == JNI_TRUE;
  if (e->ExceptionCheck()) {
    e->ExceptionClear();
    return false;
  }
  return r;
}

}  // namespace

void init() {
  JNIEnv* e = env();
  jobject a = activity();
  if (!e || !a) {
    LOGW("No se pudo obtener la actividad de Android; los anuncios quedan desactivados.");
    return;
  }
  jclass cls = e->GetObjectClass(a);
  g_show = e->GetMethodID(cls, "showRewardedAd", "()V");
  g_ready = e->GetMethodID(cls, "isRewardedReady", "()Z");
  g_consume = e->GetMethodID(cls, "consumeReward", "()Z");
  g_showing = e->GetMethodID(cls, "isAdShowing", "()Z");
  g_banner = e->GetMethodID(cls, "setBannerVisible", "(Z)V");
  e->DeleteLocalRef(cls);
  if (e->ExceptionCheck()) {
    e->ExceptionClear();
    LOGW("MainActivity no tiene los metodos de anuncios; revisa MainActivity.java.");
    return;
  }
  g_ok = g_show && g_ready && g_consume && g_showing && g_banner;
}

void shutdown() {}

bool rewardedReady() { return callBool(g_ready); }

void showRewarded() {
  JNIEnv* e = env();
  jobject a = activity();
  if (!g_ok || !e || !a) return;
  e->CallVoidMethod(a, g_show);
  if (e->ExceptionCheck()) e->ExceptionClear();
}

bool pollReward() { return callBool(g_consume); }
bool isShowing() { return callBool(g_showing); }
bool isSimulated() { return false; }
float simProgress() { return 0.0f; }

void setBannerVisible(bool visible) {
  JNIEnv* e = env();
  jobject a = activity();
  if (!g_ok || !e || !a) return;
  e->CallVoidMethod(a, g_banner, visible ? JNI_TRUE : JNI_FALSE);
  if (e->ExceptionCheck()) e->ExceptionClear();
}

// Un banner estándar mide 50 dp de alto. La pantalla lógica mide 720 unidades de ancho,
// que equivalen a unos 360-410 dp, así que 50 dp son aproximadamente 90-100 unidades.
int bannerHeight() { return 100; }

}  // namespace verity::ads
