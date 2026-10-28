// Verity - funciones de dibujo. Todo se dibuja con formas de raylib: el juego no usa imágenes.
#pragma once

#include "raylib.h"

namespace verity::gfx {

// Paleta
inline constexpr Color kInk = {43, 43, 58, 255};          // texto principal
inline constexpr Color kInkSoft = {96, 104, 128, 255};    // texto secundario
inline constexpr Color kWhite = {255, 255, 255, 255};
inline constexpr Color kPanel = {255, 255, 255, 235};
inline constexpr Color kSkyTop = {124, 214, 245, 255};
inline constexpr Color kSkyBottom = {224, 247, 255, 255};
inline constexpr Color kYellow = {255, 214, 51, 255};
inline constexpr Color kOrange = {255, 138, 61, 255};
inline constexpr Color kGreen = {53, 196, 106, 255};
inline constexpr Color kBlue = {70, 140, 240, 255};
inline constexpr Color kGrey = {185, 195, 210, 255};
inline constexpr Color kRed = {235, 70, 80, 255};

Color withAlpha(Color c, float alpha);
Color darker(Color c, float factor);

// Bola sonriente grande. `happy` (0..1) abre la boca; `blink` (0..1) cierra los ojos.
void drawSmiley(Vector2 center, float radius, int skin, float happy, float blink);

// Sonrisita pequeña para partículas y minijuegos.
void drawMiniSmiley(Vector2 center, float radius, bool golden, float alpha);

// Rectángulo redondeado con radio en píxeles.
void drawRound(Rectangle r, float radiusPx, Color fill);
void drawRoundLines(Rectangle r, float radiusPx, float thickness, Color color);

// Panel blanco con sombra suave.
void drawPanel(Rectangle r, float radiusPx = 28, Color fill = kPanel);

// Botón con "profundidad". Devuelve el rectángulo donde va el texto.
Rectangle drawButton(Rectangle r, Color fill, bool enabled, bool pressed);

// Fuente del juego (Lilita One, incluida dentro del programa). Hay que llamar a initFonts()
// después de abrir la ventana y a shutdownFonts() antes de cerrarla.
void initFonts();
void shutdownFonts();
const Font& font();

void drawText(const char* text, float x, float y, int size, Color color);
void drawTextCentered(const char* text, float cx, float y, int size, Color color);
void drawTextRight(const char* text, float rightX, float y, int size, Color color);
int textWidth(const char* text, int size);

// Texto con borde oscuro, para números flotantes.
void drawOutlinedText(const char* text, float cx, float y, int size, Color fill, Color outline);

// Iconos dibujados con formas
void drawSpeaker(Vector2 c, float s, bool on, Color color);
void drawWheelIcon(Vector2 c, float r);
void drawUpArrow(Vector2 c, float s, Color color);
void drawHatIcon(Vector2 c, float s);
void drawMedalIcon(Vector2 c, float s);
void drawLockIcon(Vector2 c, float s, Color color);

}  // namespace verity::gfx
