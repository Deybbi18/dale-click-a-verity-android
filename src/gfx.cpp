#include "gfx.hpp"

#include <algorithm>
#include <cmath>

#include "font_data.hpp"

namespace verity::gfx {

namespace {

const Color kBody = {255, 214, 51, 255};
const Color kBodyLight = {255, 228, 98, 255};
const Color kEdge = {224, 158, 0, 255};
const Color kFeature = {60, 42, 20, 255};

// raylib descarta los triángulos con el giro contrario, así que se dibujan en los dos sentidos.
void tri(Vector2 a, Vector2 b, Vector2 c, Color col) {
  DrawTriangle(a, b, c, col);
  DrawTriangle(a, c, b, col);
}

Vector2 at(Vector2 c, float r, float fx, float fy) { return {c.x + r * fx, c.y + r * fy}; }

void drawHeart(Vector2 c, float s, Color col) {
  DrawCircleV({c.x - s * 0.5f, c.y - s * 0.15f}, s * 0.55f, col);
  DrawCircleV({c.x + s * 0.5f, c.y - s * 0.15f}, s * 0.55f, col);
  tri({c.x - s * 1.02f, c.y + s * 0.12f}, {c.x + s * 1.02f, c.y + s * 0.12f}, {c.x, c.y + s * 1.25f}, col);
}

void drawSunglasses(Vector2 c, float r) {
  const Color lens = {28, 28, 40, 255};
  Rectangle left = {c.x - r * 0.70f, c.y - r * 0.40f, r * 0.60f, r * 0.40f};
  Rectangle right = {c.x + r * 0.10f, c.y - r * 0.40f, r * 0.60f, r * 0.40f};
  DrawLineEx(at(c, r, -0.68f, -0.30f), at(c, r, -0.97f, -0.36f), r * 0.05f, lens);
  DrawLineEx(at(c, r, 0.68f, -0.30f), at(c, r, 0.97f, -0.36f), r * 0.05f, lens);
  DrawRectangleRec({c.x - r * 0.12f, c.y - r * 0.33f, r * 0.24f, r * 0.06f}, lens);
  DrawRectangleRounded(left, 0.45f, 8, lens);
  DrawRectangleRounded(right, 0.45f, 8, lens);
  const Color glare = {255, 255, 255, 90};
  DrawLineEx({left.x + r * 0.12f, left.y + r * 0.30f}, {left.x + r * 0.26f, left.y + r * 0.08f}, r * 0.045f, glare);
  DrawLineEx({right.x + r * 0.12f, right.y + r * 0.30f}, {right.x + r * 0.26f, right.y + r * 0.08f}, r * 0.045f, glare);
}

void drawPartyHat(Vector2 c, float r) {
  const Color pink = {255, 92, 138, 255};
  Vector2 a = at(c, r, -0.38f, -0.90f);
  Vector2 b = at(c, r, 0.36f, -0.95f);
  Vector2 tip = at(c, r, 0.08f, -1.72f);
  tri(a, b, tip, pink);
  DrawLineEx(a, b, r * 0.07f, {255, 235, 120, 255});
  const float spots[3][3] = {{0.30f, -0.04f, 0.045f}, {0.52f, 0.06f, 0.04f}, {0.70f, -0.03f, 0.035f}};
  for (const auto& s : spots) {
    Vector2 base = {(a.x + b.x) / 2, (a.y + b.y) / 2};
    Vector2 p = {base.x + (tip.x - base.x) * s[0] + r * s[1], base.y + (tip.y - base.y) * s[0]};
    DrawCircleV(p, r * s[2] * 1.6f, WHITE);
  }
  DrawCircleV(tip, r * 0.10f, {255, 235, 120, 255});
}

void drawCrown(Vector2 c, float r) {
  const Color gold = {255, 200, 40, 255};
  const Color goldDark = {176, 112, 0, 255};
  Vector2 p0 = at(c, r, -0.42f, -0.86f), p1 = at(c, r, -0.42f, -1.30f), p2 = at(c, r, -0.21f, -1.05f);
  Vector2 p3 = at(c, r, 0.0f, -1.46f), p4 = at(c, r, 0.21f, -1.05f), p5 = at(c, r, 0.42f, -1.30f);
  Vector2 p6 = at(c, r, 0.42f, -0.86f);
  tri(p0, p6, at(c, r, 0.42f, -1.05f), gold);
  tri(p0, at(c, r, 0.42f, -1.05f), at(c, r, -0.42f, -1.05f), gold);
  tri(at(c, r, -0.42f, -1.05f), p1, p2, gold);
  tri(p2, p3, p4, gold);
  tri(p4, p5, at(c, r, 0.42f, -1.05f), gold);
  const float th = r * 0.035f;
  DrawLineEx(p0, p1, th, goldDark);
  DrawLineEx(p1, p2, th, goldDark);
  DrawLineEx(p2, p3, th, goldDark);
  DrawLineEx(p3, p4, th, goldDark);
  DrawLineEx(p4, p5, th, goldDark);
  DrawLineEx(p5, p6, th, goldDark);
  DrawLineEx(p6, p0, th, goldDark);
  DrawCircleV(p1, r * 0.055f, kRed);
  DrawCircleV(p3, r * 0.065f, kBlue);
  DrawCircleV(p5, r * 0.055f, kRed);
  DrawCircleV(at(c, r, -0.22f, -0.96f), r * 0.04f, WHITE);
  DrawCircleV(at(c, r, 0.0f, -0.96f), r * 0.04f, WHITE);
  DrawCircleV(at(c, r, 0.22f, -0.96f), r * 0.04f, WHITE);
}

void drawStar(Vector2 c, float s, Color col) {
  tri({c.x - s * 0.28f, c.y}, {c.x, c.y - s}, {c.x + s * 0.28f, c.y}, col);
  tri({c.x - s * 0.28f, c.y}, {c.x, c.y + s}, {c.x + s * 0.28f, c.y}, col);
  tri({c.x, c.y - s * 0.28f}, {c.x - s, c.y}, {c.x, c.y + s * 0.28f}, col);
  tri({c.x, c.y - s * 0.28f}, {c.x + s, c.y}, {c.x, c.y + s * 0.28f}, col);
}

}  // namespace

Color withAlpha(Color c, float alpha) {
  alpha = std::clamp(alpha, 0.0f, 1.0f);
  c.a = static_cast<unsigned char>(c.a * alpha);
  return c;
}

Color darker(Color c, float factor) {
  c.r = static_cast<unsigned char>(c.r * factor);
  c.g = static_cast<unsigned char>(c.g * factor);
  c.b = static_cast<unsigned char>(c.b * factor);
  return c;
}

void drawSmiley(Vector2 c, float r, int skin, float happy, float blink) {
  // Sombra en el piso
  DrawEllipse(static_cast<int>(c.x), static_cast<int>(c.y + r * 1.04f), r * 0.78f, r * 0.12f, {20, 60, 90, 45});

  DrawCircleV(c, r, kEdge);
  DrawCircleV(c, r * 0.94f, kBody);
  DrawCircleV({c.x - r * 0.07f, c.y - r * 0.10f}, r * 0.80f, kBodyLight);
  DrawEllipse(static_cast<int>(c.x - r * 0.42f), static_cast<int>(c.y - r * 0.55f), r * 0.20f, r * 0.11f, {255, 255, 255, 140});

  // Mejillas
  DrawCircleV(at(c, r, -0.60f, 0.17f), r * 0.11f, {255, 110, 90, 95});
  DrawCircleV(at(c, r, 0.60f, 0.17f), r * 0.11f, {255, 110, 90, 95});

  // Ojos
  const float ey = c.y - r * 0.18f;
  const float ex = r * 0.33f;
  if (skin == 3) {
    const float pulse = 1.0f + 0.10f * std::sin(static_cast<float>(GetTime()) * 6.0f);
    drawHeart({c.x - ex, ey}, r * 0.14f * pulse, {255, 59, 92, 255});
    drawHeart({c.x + ex, ey}, r * 0.14f * pulse, {255, 59, 92, 255});
  } else if (skin != 1) {
    const float eh = std::max(r * 0.02f, r * 0.17f * (1.0f - 0.9f * std::clamp(blink, 0.0f, 1.0f)));
    DrawEllipse(static_cast<int>(c.x - ex), static_cast<int>(ey), r * 0.085f, eh, kFeature);
    DrawEllipse(static_cast<int>(c.x + ex), static_cast<int>(ey), r * 0.085f, eh, kFeature);
    if (blink < 0.5f) {
      DrawCircleV({c.x - ex + r * 0.03f, ey - eh * 0.4f}, r * 0.03f, WHITE);
      DrawCircleV({c.x + ex + r * 0.03f, ey - eh * 0.4f}, r * 0.03f, WHITE);
    }
  }

  // Boca
  if (happy > 0.25f) {
    // Boca abierta: semicírculo inferior (en raylib los ángulos giran hacia abajo) con lengua.
    const Vector2 m = {c.x, c.y + r * 0.10f};
    DrawCircleSector(m, r * 0.50f, 0, 180, 40, kFeature);
    DrawEllipse(static_cast<int>(c.x), static_cast<int>(m.y + r * 0.38f), r * 0.21f, r * 0.10f, {240, 96, 110, 255});
  } else {
    const Vector2 m = {c.x, c.y + r * 0.02f};
    DrawRing(m, r * 0.49f, r * 0.56f, 30, 150, 36, kFeature);
    DrawCircleV({m.x + std::cos(30 * DEG2RAD) * r * 0.525f, m.y + std::sin(30 * DEG2RAD) * r * 0.525f}, r * 0.037f, kFeature);
    DrawCircleV({m.x + std::cos(150 * DEG2RAD) * r * 0.525f, m.y + std::sin(150 * DEG2RAD) * r * 0.525f}, r * 0.037f, kFeature);
  }

  // Accesorios
  if (skin == 1) drawSunglasses(c, r);
  if (skin == 2) drawPartyHat(c, r);
  if (skin == 4) drawCrown(c, r);
}

void drawMiniSmiley(Vector2 c, float r, bool golden, float alpha) {
  const Color edge = golden ? Color{214, 140, 0, 255} : kEdge;
  const Color body = golden ? Color{255, 196, 0, 255} : kBody;
  DrawCircleV(c, r, withAlpha(edge, alpha));
  DrawCircleV(c, r * 0.9f, withAlpha(body, alpha));
  DrawCircleV(at(c, r, -0.34f, -0.18f), r * 0.11f, withAlpha(kFeature, alpha));
  DrawCircleV(at(c, r, 0.34f, -0.18f), r * 0.11f, withAlpha(kFeature, alpha));
  DrawRing({c.x, c.y + r * 0.02f}, r * 0.40f, r * 0.52f, 30, 150, 16, withAlpha(kFeature, alpha));
  if (golden) {
    DrawCircleV(at(c, r, -0.45f, -0.55f), r * 0.12f, withAlpha(WHITE, alpha * 0.9f));
    const float t = static_cast<float>(GetTime());
    DrawCircleV(at(c, r, 0.95f * std::cos(t * 3.0f), 0.95f * std::sin(t * 3.0f)), r * 0.09f, withAlpha(WHITE, alpha));
  }
}

void drawRound(Rectangle r, float radiusPx, Color fill) {
  const float m = std::max(1.0f, std::min(r.width, r.height) / 2);
  DrawRectangleRounded(r, std::clamp(radiusPx / m, 0.0f, 1.0f), 12, fill);
}

void drawRoundLines(Rectangle r, float radiusPx, float thickness, Color color) {
  const float m = std::max(1.0f, std::min(r.width, r.height) / 2);
  DrawRectangleRoundedLinesEx(r, std::clamp(radiusPx / m, 0.0f, 1.0f), 12, thickness, color);
}

void drawPanel(Rectangle r, float radiusPx, Color fill) {
  drawRound({r.x, r.y + 6, r.width, r.height}, radiusPx, {20, 60, 90, 38});
  drawRound(r, radiusPx, fill);
}

Rectangle drawButton(Rectangle r, Color fill, bool enabled, bool pressed) {
  const Color face = enabled ? fill : kGrey;
  const float lift = pressed ? 2.0f : 7.0f;  // cuánto sobresale la cara del botón sobre su base
  drawRound({r.x, r.y + 7, r.width, r.height}, 20, darker(face, 0.72f));
  const Rectangle top = {r.x, r.y + (7.0f - lift), r.width, r.height};
  drawRound(top, 20, face);
  return top;
}

namespace {
Font g_font = {};
bool g_fontLoaded = false;
constexpr float kSpacing = 0.03f;  // espacio entre letras, como fracción del tamaño
constexpr float kTextDy = -0.17f;  // sube el texto para alinearlo con el resto de las formas
}  // namespace

void initFonts() {
  int codepoints[300];
  int n = 0;
  for (int c = 32; c <= 255; ++c) codepoints[n++] = c;
  codepoints[n++] = 0x2026;  // puntos suspensivos
  g_font = LoadFontFromMemory(".ttf", kFontData, static_cast<int>(kFontSize), 64, codepoints, n);
  if (g_font.texture.id != 0) {
    GenTextureMipmaps(&g_font.texture);
    SetTextureFilter(g_font.texture, TEXTURE_FILTER_TRILINEAR);
    g_fontLoaded = true;
  } else {
    g_font = GetFontDefault();  // por si algo falla, el juego sigue con la letra básica
  }
}

void shutdownFonts() {
  if (g_fontLoaded) UnloadFont(g_font);
  g_fontLoaded = false;
}

const Font& font() { return g_font; }

int textWidth(const char* text, int size) {
  return static_cast<int>(std::ceil(MeasureTextEx(g_font, text, static_cast<float>(size), size * kSpacing).x));
}

void drawText(const char* text, float x, float y, int size, Color color) {
  DrawTextEx(g_font, text, {x, y + size * kTextDy}, static_cast<float>(size), size * kSpacing, color);
}

void drawTextCentered(const char* text, float cx, float y, int size, Color color) {
  drawText(text, cx - textWidth(text, size) / 2.0f, y, size, color);
}

void drawTextRight(const char* text, float rightX, float y, int size, Color color) {
  drawText(text, rightX - static_cast<float>(textWidth(text, size)), y, size, color);
}

void drawOutlinedText(const char* text, float cx, float y, int size, Color fill, Color outline) {
  const float x = cx - textWidth(text, size) / 2.0f;
  const float o = static_cast<float>(std::max(2, size / 14));
  const float offs[8][2] = {{-o, 0}, {o, 0}, {0, -o}, {0, o}, {-o, -o}, {o, -o}, {-o, o}, {o, o}};
  for (const auto& d : offs) drawText(text, x + d[0], y + d[1], size, outline);
  drawText(text, x, y, size, fill);
}

void drawSpeaker(Vector2 c, float s, bool on, Color color) {
  DrawRectangleRec({c.x - s * 0.62f, c.y - s * 0.22f, s * 0.34f, s * 0.44f}, color);
  tri({c.x - s * 0.32f, c.y - s * 0.22f}, {c.x + s * 0.12f, c.y - s * 0.62f}, {c.x + s * 0.12f, c.y + s * 0.62f}, color);
  tri({c.x - s * 0.32f, c.y + s * 0.22f}, {c.x - s * 0.32f, c.y - s * 0.22f}, {c.x + s * 0.12f, c.y + s * 0.62f}, color);
  if (on) {
    DrawRing(c, s * 0.40f, s * 0.50f, -45, 45, 16, color);
    DrawRing(c, s * 0.62f, s * 0.72f, -45, 45, 16, color);
  } else {
    DrawLineEx({c.x + s * 0.28f, c.y - s * 0.32f}, {c.x + s * 0.80f, c.y + s * 0.32f}, s * 0.12f, kRed);
    DrawLineEx({c.x + s * 0.28f, c.y + s * 0.32f}, {c.x + s * 0.80f, c.y - s * 0.32f}, s * 0.12f, kRed);
  }
}

void drawWheelIcon(Vector2 c, float r) {
  const Color cols[4] = {kOrange, kYellow, kGreen, kBlue};
  for (int i = 0; i < 8; ++i) DrawCircleSector(c, r, i * 45.0f, (i + 1) * 45.0f, 8, cols[i % 4]);
  DrawCircleV(c, r * 0.18f, WHITE);
  DrawRing(c, r * 0.94f, r, 0, 360, 32, kInk);
}

void drawUpArrow(Vector2 c, float s, Color color) {
  tri({c.x - s * 0.55f, c.y - s * 0.05f}, {c.x + s * 0.55f, c.y - s * 0.05f}, {c.x, c.y - s * 0.65f}, color);
  DrawRectangleRec({c.x - s * 0.20f, c.y - s * 0.08f, s * 0.40f, s * 0.70f}, color);
}

void drawMedalIcon(Vector2 c, float s) {
  tri({c.x - s * 0.30f, c.y - s * 0.62f}, {c.x - s * 0.02f, c.y - s * 0.62f}, {c.x + s * 0.10f, c.y - s * 0.05f}, {235, 70, 80, 255});
  tri({c.x + s * 0.30f, c.y - s * 0.62f}, {c.x + s * 0.02f, c.y - s * 0.62f}, {c.x - s * 0.10f, c.y - s * 0.05f}, {70, 140, 240, 255});
  DrawCircleV({c.x, c.y + s * 0.18f}, s * 0.40f, {214, 150, 0, 255});
  DrawCircleV({c.x, c.y + s * 0.18f}, s * 0.34f, {255, 214, 51, 255});
  drawStar({c.x, c.y + s * 0.18f}, s * 0.22f, {255, 255, 255, 230});
}

void drawLockIcon(Vector2 c, float s, Color color) {
  DrawRing({c.x, c.y - s * 0.12f}, s * 0.20f, s * 0.32f, 180, 360, 16, color);
  DrawRectangleRounded({c.x - s * 0.36f, c.y - s * 0.12f, s * 0.72f, s * 0.56f}, 0.25f, 8, color);
}

void drawHatIcon(Vector2 c, float s) {
  tri({c.x - s * 0.5f, c.y + s * 0.55f}, {c.x + s * 0.5f, c.y + s * 0.55f}, {c.x, c.y - s * 0.6f}, {255, 92, 138, 255});
  DrawCircleV({c.x, c.y - s * 0.62f}, s * 0.14f, {255, 235, 120, 255});
  DrawCircleV({c.x - s * 0.08f, c.y + s * 0.1f}, s * 0.07f, WHITE);
  DrawCircleV({c.x + s * 0.12f, c.y + s * 0.32f}, s * 0.07f, WHITE);
}

}  // namespace verity::gfx
