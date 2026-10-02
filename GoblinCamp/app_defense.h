// 守城打字：怪物帶著單字走向營地，打對拼字就打倒牠。單字從目前級數隨機抽
#pragma once

struct DefenseApp : App {
  struct Mob { char w[16]; float x, v; int y; bool big; bool alive; };
  Mob mobs[8];
  char typed[16];
  int hp, score;
  float spawn, rate, elapsed;
  bool over;
  struct Fx { float x, y, life; int pts; } fx[6];
  char pool[40][16];   // 先從 SD 抽 40 個字，遊戲中不用一直讀卡
  int poolN = 0;

  bool textMode() override { return !over; }

  void fillPool() {
    poolN = 0;
    if (!vocab.ok) return;
    int a = vocab.lvFirst[cfg.level], n = vocab.size(cfg.level);
    Word w;
    for (int tries = 0; tries < 120 && poolN < 40 && n > 0; tries++) {
      if (!vocab.get(a + random(n), w)) break;
      String s = w.w; s.toLowerCase();
      bool okWord = s.length() >= 3 && s.length() <= 10;
      for (char c : s) if (c < 'a' || c > 'z') okWord = false;
      if (okWord) strlcpy(pool[poolN++], s.c_str(), 16);
    }
    if (!poolN) {   // 沒有 SD 卡就用內建的幾個字
      const char* fb[] = {"goblin", "camp", "fire", "tent", "night", "star", "moon", "sword", "shield", "forest"};
      for (auto s : fb) strlcpy(pool[poolN++], s, 16);
    }
  }
  void reset() {
    for (auto& m : mobs) m.alive = false;
    for (auto& f : fx) f.life = 0;
    typed[0] = 0; hp = 3; score = 0; spawn = 1; rate = 3.2f; elapsed = 0; over = false;
  }
  void enter() override { fillPool(); reset(); }

  Mob* target() {
    if (!typed[0]) return nullptr;
    for (auto& m : mobs) if (m.alive && !strncmp(m.w, typed, strlen(typed))) return &m;
    return nullptr;
  }

  void draw(float t, float dt) override {
    CampOpt o; o.tentX = -6; o.groundH = 26;
    drawCamp(0, 13, 240, 109, t, over ? 0 : dt, o);
    statusBar("守城打字");
    if (!over) {
      elapsed += dt; spawn -= dt;
      if (spawn <= 0) {
        for (auto& m : mobs) if (!m.alive) {
          // 避免兩隻怪物開頭字母一樣，不然打第一個字就分不出來
          const char* w = pool[random(poolN)];
          for (int k = 0; k < 10; k++) {
            bool clash = false;
            for (auto& o2 : mobs) if (o2.alive && o2.w[0] == w[0]) clash = true;
            if (!clash) break;
            w = pool[random(poolN)];
          }
          strlcpy(m.w, w, 16);
          m.x = 240; m.y = 64 + random(3) * 7; m.v = 7 + min(10.f, elapsed / 8) + random(300) / 100.f;
          m.big = strlen(w) > 7; m.alive = true;
          break;
        }
        rate = max(1.3f, rate * 0.96f); spawn = rate;
      }
    }
    Mob* tg = target();
    for (auto& m : mobs) {
      if (!m.alive) continue;
      if (!over) m.x -= m.v * dt;
      sprite(m.big ? S_OGRE : S_ZOMBIE, 8 + ((int)(t * 6)) % 4, (int)m.x, m.y, 2, FLIP);
      int ww = textWidth(m.w, F_BODYB) + 6, lx = (int)m.x + 16 - ww / 2;
      R(lx, m.y - 13, ww, 12, &m == tg ? P::ink : P::panel);
      if (&m == tg) {
        int a = text(typed, lx + 3, m.y - 3, P::amber, F_BODYB);
        text(m.w + strlen(typed), lx + 3 + a, m.y - 3, P::text, F_BODYB);
      } else text(m.w, lx + 3, m.y - 3, P::text, F_BODYB);
      if (m.x < 30 && !over) {
        m.alive = false; hp--; blip(180, 200);
        if (&m == tg) typed[0] = 0;
        if (hp <= 0) { over = true; bestScore = max(bestScore, score); }
      }
    }
    for (auto& f : fx) {
      if (f.life <= 0) continue;
      f.life -= dt * 2;
      text("+" + String(f.pts), (int)f.x, (int)(f.y - (1 - f.life) * 14), P::amber, F_SMALL);
      for (int j = 0; j < 6; j++) R((int)(f.x + cosf(j) * (1 - f.life) * 14), (int)(f.y + 10 + sinf(j) * (1 - f.life) * 10), 2, 2, P::green);
    }
    for (int i = 0; i < 3; i++) bitmap(IC_HEART, 5, 4 + i * 7, 17, i < hp ? P::red : P::line);
    text(String(score), 236, 24, P::amber, F_BODYB, RIGHT);
    R(0, 122, W, 13, P::panel2);
    text("›", 5, 132, P::amber, F_BODYB);
    text(String(typed) + ((millis() / 500) % 2 ? "_" : ""), 14, 132, P::text, F_BODYB);
    text("直接打字", 234, 132, P::dim, F_BODY, RIGHT);
    if (over) {
      R(40, 38, 160, 56, P::ink); R(40, 38, 160, 1, P::red);
      text("營地被攻破了", 120, 58, P::red, F_BODY, CENTER);
      text(String(score) + " 分 · 最高 " + bestScore, 120, 74, P::text, F_BODY, CENTER);
      text("Enter 再來一次", 120, 88, P::dim, F_BODY, CENTER);
    }
  }

  void key(const KeyEv& e) override {
    if (e.k == K_BACK) { bestScore = max(bestScore, score); go(A_HOME); return; }
    if (over) { if (e.k == K_OK) reset(); return; }
    size_t n = strlen(typed);
    if (e.k == K_DEL && n) { typed[n - 1] = 0; return; }
    if (e.k != K_CHAR) return;
    char c = tolower(e.c);
    if (c < 'a' || c > 'z' || n >= 15) return;
    typed[n] = c; typed[n + 1] = 0;
    Mob* m = target();
    if (!m) { typed[n] = 0; blip(200, 30); return; }
    blip(1200, 15);
    if (!strcmp(m->w, typed)) {
      int pts = strlen(m->w) * 10;
      score += pts; m->alive = false; typed[0] = 0; blip(1500, 50);
      for (auto& f : fx) if (f.life <= 0) { f = {m->x + 10, (float)m->y - 4, 1, pts}; break; }
    }
  }
} defenseApp;
