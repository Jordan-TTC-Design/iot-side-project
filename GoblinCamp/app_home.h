// 主選單、營火時鐘
#pragma once

struct MenuItem { const char* label; const char* const* icon; uint16_t color; AppId id; };
const MenuItem MENU[] = {
    {"英語單字", IC_BOOK, P::green, A_VOCAB},   {"今日新聞", IC_NEWS, P::blue, A_NEWS},
    {"守城打字", IC_SWORD, P::red, A_DEFENSE},  {"Claude 額度", IC_SPARK, P::claude, A_CLAUDE},
    {"正在播放", IC_NOTE, P::green, A_MUSIC},     {"遙控器", IC_REMOTE, P::pink, A_REMOTE},    {"營火時鐘", IC_FIRE, P::amber, A_CLOCK},
    {"設定", IC_GEAR, P::dim, A_SETTINGS},
};
constexpr int MENU_N = sizeof(MENU) / sizeof(MENU[0]);
int bestScore = 0;

String menuValue(AppId id) {
  switch (id) {
    case A_VOCAB: return vocab.ok ? "Lv" + String(cfg.level) + " · " + String(vocab.mastered(cfg.level) * 100 / max(1, vocab.size(cfg.level))) + "%" : String("沒有 SD");
    case A_DEFENSE: return bestScore ? String(bestScore) : String("");
    case A_CLAUDE: return usage.cur() ? String(usage.cur()->h5) + "%" : String("");
    case A_NEWS: return "";
    case A_REMOTE: return cfg.acPower ? "冷氣 " + String(cfg.acTemp) + "°" : String("");
    default: return "";
  }
}

struct HomeApp : App {
  int sel = 0, top = 0;
  float slide = 1;
  void draw(float t, float dt) override {
    drawCamp(0, 13, 104, 122, t, dt);
    R(104, 13, 1, 122, P::line); R(105, 13, 135, 122, P::panel);
    const int rowH = 24, vis = 5;
    keepVisible(sel, top, vis);
    slide += (1 - slide) * min(1.f, dt * 14);
    for (int i = 0; i < vis && top + i < MENU_N; i++) {
      int k = top + i, y = 14 + i * rowH;
      bool on = k == sel;
      int dx = on ? (int)(3 * slide) : 0;
      auto& it = MENU[k];
      if (on) { R(107 + dx, y + 2, 129, rowH - 4, P::amber); R(105, y + 2, 2, rowH - 4, P::amber); }
      bitmap(it.icon, 10, 112 + dx, y + 7, on ? P::ink : it.color);
      text(it.label, 127 + dx, y + 16, on ? P::ink : P::text);
      String v = menuValue(it.id);
      if (v.length()) text(v, 233 + dx, y + 16, on ? P::ink : P::dim, F_SMALL, RIGHT);
    }
    scrollbar(237, 14, 120, top, vis, MENU_N);
    statusBar();
  }
  void key(const KeyEv& e) override {
    if (e.k == K_UP) { sel = (sel + MENU_N - 1) % MENU_N; slide = 0; blip(660); }
    if (e.k == K_DOWN) { sel = (sel + 1) % MENU_N; slide = 0; blip(660); }
    if (e.k == K_OK) { blip(990, 40); go(MENU[sel].id); }
  }
} homeApp;

struct ClockApp : App {
  void draw(float t, float dt) override {
    CampOpt o; o.tentX = 88; o.groundH = 34; o.sleep = true;
    drawCamp(0, 0, 240, 135, t, dt, o);
    tm tt;
    bool ok = timeOk(tt);
    // 字加深色陰影，白天天空變亮也看得清楚
    auto shadowText = [](const String& s, int x, int y, uint16_t c, F f) {
      text(s, x + 1, y + 1, P::ink, f, CENTER); text(s, x, y, c, f, CENTER);
    };
    shadowText(ok ? two(tt.tm_hour) + ":" + two(tt.tm_min) : String("--:--"), 120, 42, rgb(0xfff3d6), F_CLOCK);
    shadowText(ok ? String(tt.tm_mon + 1) + "/" + tt.tm_mday + " 週" + WEEK[tt.tm_wday] : String("連上 Wi-Fi 才會對時"), 120, 58, rgb(0xe8e0f4), F_BODY);
    // 常亮開關：右上角小太陽
    if (cfg.clockAwake) {
      cv.fillCircle(8, 9, 3, P::amber);
      for (int i = 0; i < 8; i++) { float a = i * 0.785f; cv.drawPixel(8 + (int)(cosf(a) * 5.5f), 9 + (int)(sinf(a) * 5.5f), P::amber); }
      text("常亮", 16, 13, P::amber, F_SMALL);
    }
    // 營火聲
    if (fireSound.on) {
      bitmap(IC_FIRE, 10, 3, 18, P::amber);
      for (int i = 0; i < 10; i++) R(16 + i * 3, 25, 2, 2, i < cfg.fireVol ? P::amber : P::line);
    }
    if (weather.ok) { text(weather.line(), 7, 131, P::ink); text(weather.line(), 6, 130, P::text); }
    text(fireSound.on ? "f 關營火聲  [ ] 大小聲" : "f 營火聲 Space 常亮", 234, 130, P::dim, F_SMALL, RIGHT);
  }
  void key(const KeyEv& e) override {
    if (e.k == K_CHAR && (e.c == 'f' || e.c == 'F')) {
      fireSound.toggle(); toast(fireSound.on ? "營火聲開" : "營火聲關");
      if (fireSound.on && !cfg.vol) toast("音量是 0，到設定調大");
      return;
    }
    if (fireSound.on && e.k == K_CHAR && (e.c == '[' || e.c == ']')) {   // [ 小聲、] 大聲
      cfg.fireVol = constrain(cfg.fireVol + (e.c == ']' ? 1 : -1), 1, 10); saveSettings(); return;
    }
    if (e.k == K_SPACE) {
      cfg.clockAwake = !cfg.clockAwake; saveSettings(); blip(cfg.clockAwake ? 1180 : 660, 40);
      toast(cfg.clockAwake ? "營火時鐘不關螢幕" : "恢復自動關螢幕");
      return;
    }
    go(A_HOME);
  }
} clockApp;
