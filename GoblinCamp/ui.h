// 畫面工具：調色盤、字型（英文 VLW＋中文 efont 混排）、角色圖、圖示
#pragma once

M5Canvas cv(&M5Cardputer.Display);

constexpr int W = 240, H = 135;

// ─── 調色盤（跟 HTML 原型同一套夜晚營地色） ───
constexpr uint16_t rgb(uint32_t c) { return ((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x001F); }
namespace P {
constexpr uint16_t sky0 = rgb(0x100c20), sky1 = rgb(0x2b2050), panel = rgb(0x1b1530), panel2 = rgb(0x251d40),
                   line = rgb(0x3a2f62), text = rgb(0xefe6d2), dim = rgb(0x968bb4), amber = rgb(0xf2b544),
                   amberD = rgb(0x3b2a12), ink = rgb(0x1a1226), green = rgb(0x86d05e), red = rgb(0xec6a52),
                   blue = rgb(0x7ccaf2), ground = rgb(0x24391f), ground2 = rgb(0x35552c), pink = rgb(0xe9a0c4),
                   cream = rgb(0xf6eec8), spark = rgb(0xffe28a), claude = rgb(0xe08a5f);
}
uint16_t mix(uint16_t a, uint16_t b, float k) {  // k=0 → a，k=1 → b
  auto ch = [&](int sh, int mask) { int x = (a >> sh) & mask, y = (b >> sh) & mask; return (int)(x + (y - x) * k) & mask; };
  return (ch(11, 31) << 11) | (ch(5, 63) << 5) | ch(0, 31);
}

// ─── 字型 ───
struct VFont {
  lgfx::PointerWrapper data;
  lgfx::VLWfont font;
  void begin(const uint8_t* a, size_t n) { data.set(a, n); font.loadFont(&data); }
};
VFont fSmall, fBody, fBodyB, fBig, fClock;
enum F : uint8_t { F_SMALL, F_BODY, F_BODYB, F_BIG, F_CLOCK };
const lgfx::IFont* latin(F f) {
  switch (f) {
    case F_SMALL: return &fSmall.font;
    case F_BODYB: return &fBodyB.font;
    case F_BIG: return &fBig.font;
    case F_CLOCK: return &fClock.font;
    default: return &fBody.font;
  }
}
const lgfx::IFont* CJK = &fonts::efontTW_12;

void fontsBegin() {
  fSmall.begin(VLW_SMALL, sizeof(VLW_SMALL));
  fBody.begin(VLW_BODY, sizeof(VLW_BODY));
  fBodyB.begin(VLW_BODYB, sizeof(VLW_BODYB));
  fBig.begin(VLW_BIG, sizeof(VLW_BIG));
  fClock.begin(VLW_CLOCK, sizeof(VLW_CLOCK));
}

// UTF-8 解一個字，p 往前推
uint32_t utf8(const char*& p) {
  uint8_t c = *p++;
  if (c < 0x80) return c;
  int n = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : 1;
  uint32_t cp = c & (0x3F >> n);
  while (n-- && (*p & 0xC0) == 0x80) cp = (cp << 6) | (*p++ & 0x3F);
  return cp;
}
inline bool isCJK(uint32_t cp) { return cp >= 0x2E80; }

enum Align : uint8_t { LEFT, CENTER, RIGHT };

// 把字串切成「中文段／英文段」，每段換對應字型。fn(段落, 字型) 回傳寬度
template <typename Fn>
int runs(const char* s, F f, Fn fn) {
  char buf[160];
  int w = 0;
  while (*s) {
    const char* start = s;
    bool cjk = isCJK(utf8(s));
    const char* end = s;
    while (*end) {
      const char* q = end;
      if (isCJK(utf8(q)) != cjk) break;
      end = q;
    }
    size_t n = min((size_t)(end - start), sizeof(buf) - 1);
    memcpy(buf, start, n); buf[n] = 0;
    w += fn(buf, cjk ? CJK : latin(f));
    s = end;
  }
  return w;
}
int textWidth(const char* s, F f = F_BODY) {
  return runs(s, f, [](const char* b, const lgfx::IFont* font) { cv.setFont(font); return (int)cv.textWidth(b); });
}
// 以基線為準畫字（跟網頁的 alphabetic baseline 一樣），回傳寬度
int text(const char* s, int x, int y, uint16_t col, F f = F_BODY, Align a = LEFT) {
  int w = textWidth(s, f);
  if (a == CENTER) x -= w / 2;
  if (a == RIGHT) x -= w;
  cv.setTextColor(col);
  runs(s, f, [&](const char* b, const lgfx::IFont* font) {
    cv.setFont(font);
    cv.drawString(b, x, y);
    int bw = cv.textWidth(b);
    x += bw;
    return bw;
  });
  return w;
}
int text(const String& s, int x, int y, uint16_t col, F f = F_BODY, Align a = LEFT) { return text(s.c_str(), x, y, col, f, a); }

// 英文依空白斷行；中文逐字斷行。最多 maxLines 行
int wrapWords(const String& s, int maxW, F f, String* out, int maxLines) {
  int n = 0; String cur;
  int i = 0;
  while (i <= (int)s.length() && n < maxLines) {
    int j = s.indexOf(' ', i); if (j < 0) j = s.length();
    String word = s.substring(i, j);
    String t = cur.length() ? cur + " " + word : word;
    if (cur.length() && textWidth(t.c_str(), f) > maxW) { out[n++] = cur; cur = word; }
    else cur = t;
    i = j + 1;
  }
  if (cur.length() && n < maxLines) out[n++] = cur;
  return n;
}
int wrapCJK(const String& s, int maxW, F f, String* out, int maxLines) {
  int n = 0; String cur;
  const char* p = s.c_str();
  while (*p && n < maxLines) {
    const char* q = p; utf8(q);
    String ch = s.substring(p - s.c_str(), q - s.c_str());
    if (cur.length() && textWidth((cur + ch).c_str(), f) > maxW) { out[n++] = cur; cur = ""; }
    cur += ch; p = q;
  }
  if (cur.length() && n < maxLines) out[n++] = cur;
  return n;
}

// ─── 基本圖形 ───
inline void R(int x, int y, int w, int h, uint16_t c) { cv.fillRect(x, y, w, h, c); }
void bar(int x, int y, int w, int h, float frac, uint16_t c, uint16_t bg = P::panel2) {
  R(x, y, w, h, bg);
  R(x, y, (int)(w * constrain(frac, 0.f, 1.f) + .5f), h, c);
}
void bitmap(const char* const* rows, int nrows, int x, int y, uint16_t c) {
  for (int j = 0; j < nrows; j++)
    for (int i = 0; rows[j][i]; i++)
      if (rows[j][i] == '#') cv.drawPixel(x + i, y + j, c);
}

// ─── 角色圖：16×16 一格，放大 sc 倍 ───
struct Sheet { const uint16_t* px; int w, h; };
const Sheet S_WORKER{SPR_WORKER, SPR_WORKER_W, SPR_WORKER_H}, S_SCOUT{SPR_SCOUT, SPR_SCOUT_W, SPR_SCOUT_H},
    S_SAGE{SPR_SAGE, SPR_SAGE_W, SPR_SAGE_H}, S_QUEEN{SPR_QUEEN, SPR_QUEEN_W, SPR_QUEEN_H},
    S_TENT{SPR_TENT, SPR_TENT_W, SPR_TENT_H}, S_ZOMBIE{SPR_ZOMBIE, SPR_ZOMBIE_W, SPR_ZOMBIE_H},
    S_OGRE{SPR_OGRE, SPR_OGRE_W, SPR_OGRE_H};
enum Rot : uint8_t { R0, FLIP, LYING };  // LYING：往左躺下（睡覺用）
void sprite(const Sheet& s, int frame, int x, int y, int sc = 2, Rot r = R0) {
  int cols = s.w / 16, sx = (frame % cols) * 16, sy = (frame / cols) * 16;
  for (int j = 0; j < 16; j++)
    for (int i = 0; i < 16; i++) {
      uint16_t c = s.px[(sy + j) * s.w + sx + i];
      if (c == SPR_KEY) continue;
      int dx = i, dy = j;
      if (r == FLIP) dx = 15 - i;
      if (r == LYING) { dx = j; dy = 15 - i; }
      cv.fillRect(x + dx * sc, y + dy * sc, sc, sc, c);
    }
}
void image(const Sheet& s, int x, int y, int sc = 2) {
  for (int j = 0; j < s.h; j++)
    for (int i = 0; i < s.w; i++) {
      uint16_t c = s.px[j * s.w + i];
      if (c != SPR_KEY) cv.fillRect(x + i * sc, y + j * sc, sc, sc, c);
    }
}

// ─── 10×10 圖示 ───
#define ICON(name, ...) const char* const name[10] = {__VA_ARGS__};
ICON(IC_BOOK, "..........", ".###..###.", "#...##...#", "#.#.##.#.#", "#...##...#", "#.#.##.#.#", "#...##...#", "####..####", "..........", "..........")
ICON(IC_NEWS, "#########.", "#.......##", "#.###.#.##", "#.###.#.##", "#.......##", "#.#####.##", "#.#####.##", "#.......##", "#########.", "..........")
ICON(IC_SWORD, "........##", ".......###", "......###.", ".....###..", "#...###...", ".#.###....", "..###.....", ".###......", "##.#......", "#...#.....")
ICON(IC_SPARK, "....#.....", ".#..#..#..", "..#.#.#...", "...###....", "#########.", "...###....", "..#.#.#...", ".#..#..#..", "....#.....", "..........")
ICON(IC_REMOTE, ".#.#.#....", "..........", "...####...", "..#....#..", "..#.##.#..", "..#....#..", "..#.#.##..", "..#....#..", "..#.##.#..", "...####...")
ICON(IC_FIRE, "....#.....", "...##.....", "...###....", "..####.#..", "..#####...", ".###.###..", ".##...##..", ".##...##..", "..#####...", "#.#.#.#.#.")
ICON(IC_GEAR, "....##....", ".#.####.#.", "..######..", ".###..###.", "####..####", "####..####", ".###..###.", "..######..", ".#.####.#.", "....##....")
const char* const IC_HEART[5] = {".#.#.", "#####", "#####", ".###.", "..#.."};
const char* const IC_LOCK[4] = {".##.", "#..#", "####", "####"};

// ─── 提示訊息（畫在最上層） ───
struct Toast { String msg; uint32_t at = 0; } toastMsg;
void toast(const String& m) { toastMsg.msg = m; toastMsg.at = millis(); }
void drawToast() {
  if (!toastMsg.at) return;
  uint32_t age = millis() - toastMsg.at;
  if (age > 1800) { toastMsg.at = 0; return; }
  int w = textWidth(toastMsg.msg.c_str()) + 14, y = 108;
  R(120 - w / 2, y, w, 15, P::ink); R(120 - w / 2, y, 2, 15, P::amber);
  text(toastMsg.msg, 120, y + 11, P::text, F_BODY, CENTER);
}

String two(int n) { return n < 10 ? "0" + String(n) : String(n); }
const char* const WEEK[7] = {"日", "一", "二", "三", "四", "五", "六"};
