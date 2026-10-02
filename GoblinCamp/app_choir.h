// 哥布林合唱團的畫面：舞台、四種模式（唱名、錄音、自訂、遊戲）、鍵盤當鋼琴。嗓音引擎在 choir.h
#pragma once

struct ChoirApp : App {
  int mode = 0;
  bool armRec = false;
  float hop[7] = {0};
  bool has[2][7];   // 錄音／自訂音檔是否存在

  bool textMode() override { return true; }   // 每個字母鍵都是琴鍵
  static constexpr int MODES = 4;              // 唱名、錄音、自訂、遊戲
  void scan() {
    for (int i = 0; i < 7; i++) { has[0][i] = sdOk && SD.exists(choir.recPath(i)); has[1][i] = sdOk && SD.exists(choir.customPath(i)); }
  }
  void enter() override {
    static bool built = false;
    if (!built) { choir.build(); built = true; }
    scan();
    rhythm.scanSD();
  }
  void leave() override { M5Cardputer.Speaker.stop(Choir::CH); for (auto& x : choir.v) { x.on = false; if (x.f) x.f.close(); } }

  void stage(float t, float dt) {
    // 舞台：深色幕、聚光燈、木地板
    for (int y = 13; y < 104; y++) R(0, y, W, 1, mix(rgb(0x2a1530), rgb(0x12101e), (y - 13) / 91.f));
    for (int i = 0; i < W; i += 30) R(i, 13, 2, 91, rgb(0x341a3a));   // 布幕摺痕
    R(0, 104, W, 18, rgb(0x5a3a22));
    for (int i = 0; i < W; i += 24) R(i, 104, 1, 18, rgb(0x4a2e1a));
    R(0, 104, W, 1, rgb(0x8a6040));
    for (int c = 0; c < 7; c++) {
      int x = 4 + c * 33;
      bool sing = choir.singing(c);
      hop[c] = sing ? min(1.f, hop[c] + dt * 12) : max(0.f, hop[c] - dt * 5);
      if (sing) {   // 聚光燈
        for (int y = 30; y < 104; y++) { int hw = 6 + (y - 30) / 5; R(x + 16 - hw, y, hw * 2, 1, mix(rgb(0x1a1426), rgb(0x6a5a3a), 0.35f)); }
      }
      int y = 72 - (int)(sinf(hop[c] * PI) * 6);
      sprite(*SINGERS[c].s, sing ? ((int)(t * 8)) % 2 : 0, x, y, 2);
      if (sing) {   // 飄出音符
        float k = fmodf(t * 1.2f + c * 0.3f, 1.f);
        bitmap(IC_NOTE, 10, x + 22 + (int)(sinf(k * 6) * 2), 60 - (int)(k * 30), P::amber);
      }
      // 名牌
      const char* lab;
      String l;
      if (mode == 0) lab = SOLFEGE[c];
      else { l = String(c + 1); lab = l.c_str(); }
      uint16_t col = mode == 0 ? P::text : has[mode - 1][c] ? P::green : P::line;
      text(lab, x + 16, 117, sing ? P::amber : col, mode == 0 ? F_SMALL : F_BODYB, CENTER);
    }
  }

  // 遊戲畫面下方的角色：跟著打中的音跳
  void gameSingers(float t, float dt) {
    R(0, 96, W, 39, rgb(0x2a1c14));
    for (int c = 0; c < 7; c++) {
      int x = 4 + c * 33;
      rhythm.hitFlash[c] = max(0.f, rhythm.hitFlash[c] - dt * 3);
      bool sing = choir.singing(c);
      hop[c] = sing ? min(1.f, hop[c] + dt * 12) : max(0.f, hop[c] - dt * 5);
      sprite(*SINGERS[c].s, sing ? ((int)(t * 8)) % 2 : 0, x, 101 - (int)(sinf(hop[c] * PI) * 4), 2);
    }
  }

  void draw(float t, float dt) override {
    if (mode == 3) {
      rhythm.update();
      if (rhythm.phase == Rhythm::PLAY) { rhythm.drawPlay(t); gameSingers(t, dt); statusBar("音樂遊戲"); return; }
      if (rhythm.phase == Rhythm::RESULT) { rhythm.drawResult(); statusBar("音樂遊戲"); return; }
      rhythm.drawSelect();
    } else stage(t, dt);
    statusBar("哥布林合唱團");
    const char* names[MODES] = {"唱名", "錄音", "自訂", "遊戲"};
    int x = 6;
    for (int i = 0; i < MODES; i++) {
      int w = textWidth(names[i]) + 10;
      R(x, 16, w, 13, i == mode ? P::amber : P::panel2);
      text(names[i], x + w / 2, 26, i == mode ? P::ink : P::dim, F_BODY, CENTER);
      x += w + 3;
    }
    if (mode == 3) return;   // 選歌畫面有自己的提示列
    R(0, 122, W, 13, P::panel2);
    const char* hint = armRec ? "按任一個琴鍵，錄那個角色" :
                       mode == 0 ? "四排鍵＝四個八度 · Shift 升半音" :
                       mode == 1 ? "Enter 再按琴鍵錄音 · 綠色＝已錄" :
                                   "放 /goblin/choir/custom/1–7.wav";
    text(hint, 6, 132, armRec ? P::red : P::dim);
  }

  // 鍵 → 角色（0–6）與相對於中音那個八度差幾個半音；回傳 false 代表不是琴鍵
  bool keyNote(char c, int& who, int& semis) {
    struct Row { const char* keys; const char* shifted; int octave; };
    static const Row rows[4] = {
        {"zxcvbnm,./", "ZXCVBNM<>?", -1},
        {"asdfghjkl;'", "ASDFGHJKL:\"", 0},
        {"qwertyuiop[]\\", "QWERTYUIOP{}|", 1},
        {"1234567890-=", "!@#$%^&*()_+", 2},
    };
    for (auto& r : rows) {
      const char* p = strchr(r.keys, c);
      bool sharp = false;
      if (!p || !*p) { p = strchr(r.shifted, c); sharp = p && *p; if (sharp) p = r.keys + (p - r.shifted); }
      if (!p || !*p) continue;
      int d = p - r.keys;
      who = d % 7;
      semis = 12 * (r.octave + d / 7) + (sharp ? 1 : 0);
      return true;
    }
    return false;
  }

  void doRecord(int who) {
    armRec = false;
    for (int n = 3; n >= 1; n--) {   // 倒數 3 2 1
      stage(0, 0); statusBar("哥布林合唱團");
      R(70, 40, 100, 50, P::ink); R(70, 40, 100, 1, P::red);
      text(String(SINGERS[who].name) + " 要錄音", 120, 58, P::text, F_BODY, CENTER);
      text(String(n), 120, 82, P::red, F_BIG, CENTER);
      cv.pushSprite(0, 0); blip(880, 80); delay(700);
    }
    bool ok = choir.record(who, [&](float lv) {
      R(70, 40, 100, 50, P::ink); R(70, 40, 100, 1, P::red);
      cv.fillCircle(84, 58, 4, P::red); text("錄音中", 92, 62, P::red);
      bar(80, 72, 80, 6, min(1.f, lv * 3), P::green);
      cv.pushSprite(0, 0);
    });
    scan();
    toast(ok ? String(SINGERS[who].name) + " 錄好了" : String("錄音失敗：要有 SD 卡"));
    if (ok) choir.play(who, 1, 0);
  }

  void key(const KeyEv& e) override {
    if (mode == 3 && rhythm.phase != Rhythm::SELECT) {   // 遊戲中
      if (e.k == K_BACK) { rhythm.phase = Rhythm::SELECT; return; }
      if (rhythm.phase == Rhythm::RESULT) { if (e.k == K_OK) rhythm.start(rhythm.sel, false); return; }
      int who, semis;
      if (e.k == K_CHAR && keyNote(e.c, who, semis)) rhythm.press(DEGREE_SEMI[who] + semis);
      return;
    }
    if (e.k == K_BACK) { go(A_HOME); return; }
    if (e.k == K_TAB) { mode = (mode + 1) % MODES; armRec = false; scan(); blip(740); return; }
    if (mode == 3) {   // 選歌：文字模式下 ; . 是原本的字元
      if (e.k == K_CHAR && (e.c == ';' || e.c == '.')) { int n = rhythm.total(); rhythm.sel = (rhythm.sel + (e.c == '.' ? 1 : n - 1)) % n; blip(660); }
      if (e.k == K_OK) rhythm.start(rhythm.sel, false);
      if (e.k == K_SPACE || (e.k == K_CHAR && e.c == ' ')) rhythm.start(rhythm.sel, true);
      return;
    }
    if (e.k == K_OK && mode == 1) { armRec = !armRec; return; }
    if (e.k != K_CHAR) return;
    int who, semis;
    if (!keyNote(e.c, who, semis)) return;
    if (armRec) { doRecord(who); return; }
    if (mode > 0 && !has[mode - 1][who]) {
      toast(mode == 1 ? String(SINGERS[who].name) + " 還沒錄音：按 r 再按這個鍵" : String(who + 1) + ".wav 還沒放");
      return;
    }
    choir.play(who, mode, semis);
    choir.tick();
  }
} choirApp;
