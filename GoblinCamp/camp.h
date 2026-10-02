// 營地場景：夜空、星星、月亮、帳篷營火、公主、走來走去的哥布林。主選單、營火時鐘、守城遊戲共用
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

void drawCamp(int x0, int y0, int w, int h, float t, float dt, CampOpt o = CampOpt()) {
  cv.setClipRect(x0, y0, w, h);
  int gy = y0 + h - o.groundH;
  // 夜空漸層
  for (int y = y0; y < gy; y++) R(x0, y, w, 1, mix(P::sky0, P::sky1, (float)(y - y0) / h));
  for (auto& s : stars) {
    if (s.x >= w || s.y + y0 - 13 >= gy) continue;
    float a = fabsf(sinf(t * 0.8f + s.p));
    cv.drawPixel(x0 + s.x, s.y + y0 - 13, mix(P::sky1, rgb(0xfff7d6), 0.3f + 0.7f * a));
  }
  int mx = x0 + w - 16, my = y0 + 13;
  cv.fillCircle(mx, my, 5, P::cream);
  cv.fillCircle(mx + 3, my - 2, 5, mix(P::sky0, P::sky1, 0.05f));

  int tx = x0 + o.tentX, ty = gy - 44, fx = tx + 10, fy = gy - 6;
  // 營火的光暈：天空和地面各畫一半，顏色先混好（畫布沒有透明度）
  int gr = 13 + (int)(sinf(t * 9) * 1.5f);
  cv.setClipRect(x0, y0, w, gy - y0);
  cv.fillCircle(fx, fy, gr, mix(P::sky1, P::amber, 0.18f));
  cv.setClipRect(x0, gy, w, y0 + h - gy);
  R(x0, gy, w, o.groundH, P::ground);
  cv.fillCircle(fx, fy, gr, mix(P::ground, P::amber, 0.2f));
  for (int i = 0; i < w; i += 7) R(x0 + i, gy, 3, 1, P::ground2);
  for (int i = 3; i < w; i += 13) R(x0 + i, gy + 8 + (i % 3) * 5, 2, 1, P::ground2);
  cv.setClipRect(x0, y0, w, h);
  image(S_TENT, tx, ty, 2);

  // 火星往上飄
  if (random(1000) < dt * 6000) {
    for (auto& s : sparks) if (s.life <= 0) { s = {(float)fx + random(-3, 4), (float)fy - 8, 1}; break; }
  }
  for (int i = 0; i < 16; i++) {
    auto& s = sparks[i];
    if (s.life <= 0) continue;
    s.y -= dt * 14; s.x += sinf(t * 5 + i) * dt * 4; s.life -= dt * 0.9f;
    if (s.life > 0) cv.drawPixel((int)s.x, (int)s.y, s.life > 0.5f ? P::spark : P::red);
  }

  // 公主坐在火邊
  sprite(S_QUEEN, o.sleep ? 0 : ((int)(t * 2)) % 4, tx + 22, gy - 30, 2);

  for (int gi = 0; gi < 3; gi++) {
    auto& g = gobs[gi];
    if (o.sleep) {   // 睡覺時排成一排，不要疊在一起
      int sx = x0 + 2 + gi * 28;
      sprite(*g.s, 0, sx, gy - 22, 2, LYING);
      float zt = fmodf(t * 0.8f + gi * 0.7f, 2.f);
      text("z", sx + 26 + (int)(zt * 4), gy - 16 - (int)(zt * 8), P::blue, F_SMALL);
      continue;
    }
    if (g.x > w - 32) g.x = w - 32;
    if (g.pause > 0) {
      g.pause -= dt;
      sprite(*g.s, ((int)(t * 1.5f)) % 2, x0 + (int)g.x, gy - 30, 2);
      continue;
    }
    g.x += g.dir * g.v * dt;
    if (g.x > w - 32) { g.x = w - 32; g.dir = -1; g.pause = 1 + random(200) / 100.f; }
    if (g.x < 0) { g.x = 0; g.dir = 1; g.pause = 1 + random(200) / 100.f; }
    sprite(*g.s, 8 + ((int)(t * 8)) % 4, x0 + (int)g.x, gy - 30, 2, g.dir < 0 ? FLIP : R0);
  }
  cv.clearClipRect();
}
