cmake_minimum_required(VERSION 3.22)
project(Verity LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)

option(VERITY_BUILD_GAME "Compilar el juego (descarga raylib)" ON)
option(VERITY_BUILD_TESTS "Compilar las pruebas del núcleo" ON)
option(VERITY_DEV_SHOTS "Solo para desarrollo: guardar capturas de cada pantalla" OFF)

# --- Núcleo del juego: C++ puro, sin gráficos ---
add_library(verity_core STATIC src/game.cpp)
target_include_directories(verity_core PUBLIC src)
set_target_properties(verity_core PROPERTIES POSITION_INDEPENDENT_CODE ON)  # Android lo enlaza dentro de una .so
if(MSVC)
  target_compile_options(verity_core PRIVATE /W4 /utf-8)
else()
  target_compile_options(verity_core PRIVATE -Wall -Wextra)
endif()

if(VERITY_BUILD_TESTS AND NOT ANDROID)
  enable_testing()
  add_executable(verity_tests tests/test_game.cpp)
  target_link_libraries(verity_tests PRIVATE verity_core)
  add_test(NAME verity_tests COMMAND verity_tests)
endif()

if(NOT VERITY_BUILD_GAME)
  return()
endif()

# --- raylib ---
include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
if(ANDROID)
  set(PLATFORM "Android" CACHE STRING "" FORCE)
endif()
FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG 5.5
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

# --- El juego ---
set(VERITY_SOURCES src/main.cpp src/gfx.cpp src/platform.cpp src/font_data.cpp)
if(ANDROID)
  list(APPEND VERITY_SOURCES src/ads_android.cpp)
  # La app de Android carga una biblioteca compartida llamada "verity" (libverity.so).
  add_library(verity SHARED ${VERITY_SOURCES})
  target_include_directories(verity PRIVATE ${ANDROID_NDK}/sources/android/native_app_glue)
  target_link_libraries(verity PRIVATE verity_core raylib log android EGL GLESv2 OpenSLES m dl)
  # Obliga a incluir el punto de entrada de la actividad nativa (viene dentro de raylib).
  target_link_options(verity PRIVATE -u ANativeActivity_onCreate "-Wl,-z,max-page-size=16384")
else()
  list(APPEND VERITY_SOURCES src/ads_desktop.cpp)
  add_executable(verity ${VERITY_SOURCES})
  target_link_libraries(verity PRIVATE verity_core raylib)
  if(MSVC)
    target_compile_options(verity PRIVATE /utf-8)
    # Ventana sin consola
    target_link_options(verity PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
  elseif(MINGW)
    # Un solo .exe, sin DLL extra, y sin ventana de consola
    target_link_options(verity PRIVATE -static -static-libgcc -static-libstdc++ -mwindows)
  endif()
endif()

if(VERITY_DEV_SHOTS)
  target_compile_definitions(verity PRIVATE VERITY_DEV_SHOTS)
endif()
