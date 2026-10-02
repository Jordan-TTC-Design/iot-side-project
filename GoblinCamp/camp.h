// 營地場景：天空、帳篷營火、公主、走來走去的哥布林。主選單、營火時鐘、守城遊戲共用
// 跟著真實天氣（weather.h）變：白天／晚上、雲、雨、雪、霧、打雷；冷了發抖、熱了冒汗
#pragma once

struct Gob { const Sheet* s; float x; int8_t dir; float v, pause; };
Gob gobs[3] = {{&S_WORKER, 8, 1, 9, 0}, {&S_SCOUT, 60, -1, 14, 1.5f}, {&S_SAGE, 30, 1, 6, 3}};
struct Spark { float x, y, life; };
Spark sparks[16];
struct Star { uint8_t x, y; float p; };
Star stars[28];

void campInit() {
  for (int i = 0; i < 28; i++) stars[i] = {(uint8_t)((i * 53) % 240), (uint8_t)(14 + (i * 37) % 70), i * 0.7f};
}

struct CampOpt { int tentX = 22; int groundH = 30; bool sleep = false; };

// 天亮程度：有天氣資料就照日出日落，沒有就照時鐘（6～18 點）
float daylightNow() {
  if (weather.ok) return weather.daylight(epochNow());
  tm t;
  if (!getLocalTime(&t, 0) || t.tm_year < 120) return 0;
  float hr = t.tm_hour + t.tm_min / 60.f;
  return constrain(min(hr - 5.5f, 18.5f - hr), 0.f, 1.f);
}

void drawClouds(int x0, int y0, int w, float t, int n, uint16_t c, uint16_t shade) {
  for (int i = 0; i < n; i++) {
    int span = w + 60;
    int cx = x0 - 30 + (int)fmodf(i * 97 + t * (3 + i % 3), span), cy = y0 + 8 + (i * 13) % 26;
    cv.fillCircle(cx, cy + 2, 6, shade);
    cv.fillCircle(cx + 7, cy - 1, 8, c); cv.fillCircle(cx - 6, cy + 1, 6, c); cv.fillCircle(cx + 15, cy + 2, 5, c);
    R(cx - 10, cy + 3, 30, 4, c);
  }
}

void drawCamp(int x0, int y0, int w, int h, float t, float dt, CampOpt o = CampOpt()) {
  cv.setClipRect(x0, y0, w, h);
  int gy = y0 + h - o.groundH;
  float day = daylightNow();
  Sky sky = weather.sky();
  bool overcast = sky != SKY_CLEAR;
  float gloom = sky == SKY_STORM ? 0.55f : sky == SKY_RAIN ? 0.4f : sky == SKY_CLOUDY ? 0.25f : sky == SKY_FOG ? 0.3f : 0;
  // 打雷：偶爾整片天空閃一下
  bool flash = sky == SKY_STORM && fmodf(t, 6.3f) < 0.09f;

  // 天空：夜（紫）→ 黃昏（橘）→ 白天（藍），陰雨時變灰
  uint16_t topN = P::sky0, botN = P::sky1, topD = rgb(0x4a8fd8), botD = rgb(0xa9d6f2), dusk = rgb(0xe08a5f);
  for (int y = y0; y < gy; y++) {
    float k = (float)(y - y0) / h;
    uint16_t c = mix(mix(topN, botN, k), mix(topD, botD, k), day);
    if (day > 0.05f && day < 0.95f) c = mix(c, dusk, (1 - fabsf(day - 0.5f) * 2) * k * 0.6f);   // 黃昏地平線偏橘
    if (gloom) c = mix(c, mix(rgb(0x2a2a36), rgb(0x8a929c), day), gloom);
    if (flash) c = mix(c, rgb(0xe8ecff), 0.7f);
    R(x0, y, w, 1, c);
  }
  // 星星：晚上、沒有雲才看得到
  if (day < 0.6f && !overcast) {
    for (auto& s : stars) {
      if (s.x >= w || s.y + y0 - 13 >= gy) continue;
      float a = fabsf(sinf(t * 0.8f + s.p)) * (1 - day / 0.6f);
      cv.drawPixel(x0 + s.x, s.y + y0 - 13, mix(P::sky1, rgb(0xfff7d6), 0.2f + 0.8f * a));
    }
  }
  // 太陽或月亮（陰雨看不到）
  int mx = x0 + w - 16, my = y0 + 13;
  if (sky == SKY_CLEAR || sky == SKY_CLOUDY) {
    if (day >= 0.5f) {
      cv.fillCircle(mx, my, 5, rgb(0xffd75e));
      for (int i = 0; i < 8; i++) { float a = i * 0.785f + t * 0.2f; cv.drawPixel(mx + (int)(cosf(a) * 8), my + (int)(sinf(a) * 8), rgb(0xffd75e)); }
    } else {
      cv.fillCircle(mx, my, 5, P::cream);
      cv.fillCircle(mx + 3, my - 2, 5, mix(P::sky0, P::sky1, 0.05f));
    }
  }
  if (overcast && sky != SKY_FOG) {
    uint16_t cc = mix(rgb(0x3a3550), rgb(0xe9eef2), day), sh = mix(rgb(0x2a2640), rgb(0xb8c2cc), day);
    if (sky != SKY_CLOUDY) { cc = mix(cc, rgb(0x55596a), 0.5f); sh = mix(sh, rgb(0x3a3d4a), 0.5f); }
    drawClouds(x0, y0, w, t, sky == SKY_CLOUDY ? 3 : 5, cc, sh);
  }

  // 霧：一層一層飄在天空，畫在角色後面，不遮到人
  if (sky == SKY_FOG) {
    uint16_t fc = mix(rgb(0x6a6e80), rgb(0xd6dde4), day);
    for (int y = y0 + 14; y < gy - 34; y += 2)
      for (int x = x0 + ((y / 2 + (int)(t * 4)) % 3); x < x0 + w; x += 3) cv.drawPixel(x, y, fc);
  }

  int tx = x0 + o.tentX, ty = gy - 44, fx = tx + 10, fy = gy - 6;
  uint16_t skyAtFire = mix(P::sky1, rgb(0xa9d6f2), day), groundC = mix(P::ground, rgb(0x4f7d3a), day);
  // 營火的光暈：晚上明顯、白天幾乎看不到
  int gr = 13 + (int)(sinf(t * 9) * 1.5f);
  float glow = 0.2f * (1 - day * 0.8f);
  cv.setClipRect(x0, y0, w, gy - y0);
  if (!gloom) cv.fillCircle(fx, fy, gr, mix(skyAtFire, P::amber, glow));
  cv.setClipRect(x0, gy, w, y0 + h - gy);
  R(x0, gy, w, o.groundH, groundC);
  cv.fillCircle(fx, fy, gr, mix(groundC, P::amber, glow));
  uint16_t tuft = sky == SKY_SNOW ? rgb(0xe8eef4) : mix(P::ground2, rgb(0x6a9c4a), day);
  for (int i = 0; i < w; i += 7) R(x0 + i, gy, 3, 1, tuft);
  for (int i = 3; i < w; i += 13) R(x0 + i, gy + 8 + (i % 3) * 5, 2, 1, tuft);
  cv.setClipRect(x0, y0, w, h);
  image(S_TENT, tx, ty, 2);

  // 火星往上飄
  int want = random(1000) < dt * 6000 ? 1 : 0;
  if (fireBurst > 0) { want += 3 + random(3); fireBurst--; }   // 營火聲劈啪一下，就噴一撮
  for (auto& s : sparks) {
    if (!want) break;
    if (s.life <= 0) { s = {(float)fx + random(-4, 5), (float)fy - 6 - random(4), 1}; want--; }
  }
  for (int i = 0; i < 16; i++) {
    auto& s = sparks[i];
    if (s.life <= 0) continue;
    s.y -= dt * 14; s.x += sinf(t * 5 + i) * dt * 4; s.life -= dt * 0.9f;
    if (s.life > 0) cv.drawPixel((int)s.x, (int)s.y, s.life > 0.5f ? P::spark : P::red);
  }

  // 冷了發抖、熱了冒汗
  bool cold = weather.ok && weather.temp < 15, hot = weather.ok && weather.temp > 30;
  auto shake = [&](int i) { return cold ? (((int)(t * 18) + i) % 2) : 0; };
  auto sweat = [&](int x, int y, int i) {
    if (!hot) return;
    float k = fmodf(t * 0.9f + i * 0.4f, 1.f);
    R(x + 26, y + 6 + (int)(k * 6), 2, 2, P::blue);
  };

  // 公主坐在火邊
  int qx = tx + 22 + shake(9), qy = gy - 30;
  sprite(S_QUEEN, o.sleep ? 0 : ((int)(t * 2)) % 4, qx, qy, 2);
  if (!o.sleep) sweat(qx, qy, 9);

  for (int gi = 0; gi < 3; gi++) {
    auto& g = gobs[gi];
    if (o.sleep) {   // 睡覺時排成一排，不要疊在一起
      int sx = x0 + 2 + gi * 28;
      sprite(*g.s, 0, sx + shake(gi), gy - 22, 2, LYING);
      float zt = fmodf(t * 0.8f + gi * 0.7f, 2.f);
      text("z", sx + 26 + (int)(zt * 4), gy - 16 - (int)(zt * 8), P::blue, F_SMALL);
      continue;
    }
    if (g.x > w - 32) g.x = w - 32;
    int gx = x0 + (int)g.x + shake(gi), gyy = gy - 30;
    if (g.pause > 0) {
      g.pause -= dt * (cold ? 0.5f : 1);   // 冷的時候站著發抖比較久
      sprite(*g.s, ((int)(t * 1.5f)) % 2, gx, gyy, 2);
      sweat(gx, gyy, gi);
      continue;
    }
    g.x += g.dir * g.v * dt;
    if (g.x > w - 32) { g.x = w - 32; g.dir = -1; g.pause = 1 + random(200) / 100.f; }
    if (g.x < 0) { g.x = 0; g.dir = 1; g.pause = 1 + random(200) / 100.f; }
    sprite(*g.s, 8 + ((int)(t * 8)) % 4, gx, gyy, 2, g.dir < 0 ? FLIP : R0);
    sweat(gx, gyy, gi);
  }

  // 雨、雪、霧畫在最上層
  if (sky == SKY_RAIN || sky == SKY_STORM) {
    int n = (int)(weather.rain() * 70);
    uint16_t rc = mix(rgb(0x8fa8d8), rgb(0xd8e6f6), day);
    for (int i = 0; i < n; i++) {
      float sp = 110 + (i % 5) * 12;
      int x = x0 + (i * 53 + (i * i) % 17) % (w + 20) - 10;
      int y = y0 + (int)fmodf(i * 29 + t * sp, h + 10) - 10;
      x -= (y - y0) / 6;
      cv.drawLine(x, y, x - 1, y + 4, rc);
      if (y > gy && y < gy + 6 && (i % 3) == 0) cv.drawPixel(x - 1, gy + 1, rc);   // 濺起來
    }
  } else if (sky == SKY_SNOW) {
    for (int i = 0; i < 45; i++) {
      float sp = 10 + (i % 4) * 4;
      int x = x0 + (i * 47) % w + (int)(sinf(t * 1.3f + i) * 3);
      int y = y0 + (int)fmodf(i * 31 + t * sp, h);
      R(x, y, (i % 3) ? 1 : 2, (i % 3) ? 1 : 2, rgb(0xf4f8ff));
    }
  }
  cv.clearClipRect();
}
