// 設定、Wi-Fi（掃描、輸入密碼、連線）
#pragma once

const char* const SLEEP_LABEL[4] = {"30 秒", "1 分", "5 分", "永不"};
const char* const BRANDS[4] = {"日立", "大金", "國際牌", "三菱"};

struct SettingsApp : App {
  enum Item : uint8_t { S_WIFI, S_VOL, S_BRIGHT, S_SLEEP, S_IDLE, S_LEVEL, S_AC, S_SD, S_ABOUT, S_N };
  int sel = 0, top = 0;

  const char* label(int i) {
    static const char* const L[S_N] = {"Wi-Fi", "音量", "螢幕亮度", "自動關螢幕", "閒置 45 秒顯示營火時鐘",
                                       "英文程度", "冷氣品牌", "SD 卡", "關於"};
    return L[i];
  }
  String value(int i) {
    switch (i) {
      case S_WIFI: return WiFi.status() == WL_CONNECTED ? WiFi.SSID() : cfg.ssid.length() ? cfg.ssid + "（未連上）" : String("未設定 ›");
      case S_SLEEP: return SLEEP_LABEL[cfg.sleep];
      case S_IDLE: return cfg.idleClock ? "開" : "關";
      case S_LEVEL: return "Lv" + String(cfg.level) + "（解鎖到 " + cfg.unlocked + "）";
      case S_AC: return BRANDS[cfg.acBrand];
      case S_SD: return !sdOk ? String("沒有讀到") : vocab.ok ? String(vocab.count) + " 字" : String("缺單字檔");
      case S_ABOUT: return "v0.1 · " + String(ESP.getFreeHeap() / 1024) + "KB 可用";
      default: return "";
    }
  }
  bool adjustable(int i) { return i == S_VOL || i == S_BRIGHT || i == S_SLEEP || i == S_IDLE || i == S_LEVEL || i == S_AC; }
  void adjust(int i, int d) {
    switch (i) {
      case S_VOL: cfg.vol = constrain(cfg.vol + d, 0, 10); applySettings(); blip(880); break;
      case S_BRIGHT: cfg.bright = constrain(cfg.bright + d, 1, 10); applySettings(); break;
      case S_SLEEP: cfg.sleep = (cfg.sleep + d + 4) % 4; break;
      case S_IDLE: cfg.idleClock = !cfg.idleClock; break;
      case S_LEVEL: {
        int lv = cfg.level + d;
        if (lv > cfg.unlocked) { toast("第 " + String(lv) + " 級要先解鎖"); return; }
        if (lv >= 3 && lv <= 6) { vocab.save(); cfg.level = lv; }
        break;
      }
      case S_AC: cfg.acBrand = (cfg.acBrand + d + 4) % 4; cfg.acModel = 0; break;
    }
    saveSettings();
  }

  void draw(float, float) override {
    R(0, 0, W, H, P::panel);
    statusBar("設定");
    const int vis = 8, rh = 14;
    keepVisible(sel, top, vis);
    for (int i = 0; i < vis && top + i < S_N; i++) {
      int k = top + i, y = 15 + i * rh;
      bool on = k == sel;
      if (on) R(2, y, 234, rh - 1, P::amberD);
      text(label(k), 8, y + 11, on ? P::amber : P::text);
      if (k == S_VOL || k == S_BRIGHT) {
        int v = k == S_VOL ? cfg.vol : cfg.bright;
        bar(150, y + 4, 60, 4, v / 10.f, on ? P::amber : P::dim);
        text(String(v), 230, y + 11, P::text, F_SMALL, RIGHT);
      } else {
        String v = value(k);
        if (on && adjustable(k)) v = "‹ " + v + " ›";
        text(v, 230, y + 11, on ? P::text : P::dim, F_BODY, RIGHT);
      }
    }
    scrollbar(237, 15, 112, top, vis, S_N);
  }
  void key(const KeyEv& e) override {
    if (e.k == K_UP) { sel = (sel + S_N - 1) % S_N; blip(660); }
    if (e.k == K_DOWN) { sel = (sel + 1) % S_N; blip(660); }
    if ((e.k == K_LEFT || e.k == K_RIGHT) && adjustable(sel)) adjust(sel, e.k == K_RIGHT ? 1 : -1);
    if (e.k == K_OK) {
      if (sel == S_WIFI) go(A_WIFI);
      else if (adjustable(sel)) adjust(sel, 1);
      else if (sel == S_SD) { sdBegin(); vocab.load(); toast(vocab.ok ? "讀到 " + String(vocab.count) + " 個字" : String("還是讀不到")); }
    }
    if (e.k == K_BACK) go(A_HOME);
  }
} settingsApp;

struct WifiApp : App {
  enum Mode : uint8_t { SCAN, LIST, PASS, JOIN } mode = SCAN;
  int sel = 0, top = 0, n = 0;
  String ssid[16]; int rssi[16]; bool lock[16];
  String pw;
  uint32_t joinAt = 0;

  bool textMode() override { return mode == PASS; }
  void enter() override { startScan(); }
  void startScan() {
    mode = SCAN; sel = top = 0;
    WiFi.mode(WIFI_STA);
    WiFi.scanDelete();
    WiFi.scanNetworks(true);
  }
  void poll() {
    if (mode != SCAN) return;
    int r = WiFi.scanComplete();
    if (r < 0) return;
    n = 0;
    for (int i = 0; i < r && n < 16; i++) {
      String s = WiFi.SSID(i);
      if (!s.length()) continue;
      bool dup = false;
      for (int k = 0; k < n; k++) if (ssid[k] == s) dup = true;
      if (dup) continue;
      ssid[n] = s; rssi[n] = WiFi.RSSI(i); lock[n] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN; n++;
    }
    WiFi.scanDelete();
    mode = LIST;
  }
  void join(const String& s, const String& p) {
    cfg.ssid = s; cfg.pass = p; saveSettings();
    WiFi.disconnect(); wifiConnect();
    mode = JOIN; joinAt = millis();
  }

  void draw(float t, float) override {
    poll();
    R(0, 0, W, H, P::panel);
    statusBar("Wi-Fi");
    if (mode == SCAN) {
      int r = ((int)(t * 30)) % 30;
      cv.drawCircle(120, 70, r, mix(P::panel, P::blue, 1 - r / 30.f));
      sprite(S_SCOUT, ((int)(t * 4)) % 4, 104, 54, 2);
      text("搜尋中…", 120, 112, P::dim, F_BODY, CENTER);
      return;
    }
    if (mode == PASS) {
      text(ssid[sel], 8, 32, P::amber);
      text("輸入密碼，Enter 連線", 8, 50, P::dim);
      R(6, 60, 228, 20, P::panel2);
      String shown = pw + ((millis() / 500) % 2 ? "_" : "");
      text(shown, 10, 74, P::text, F_BODY);
      text("` 取消", 234, 130, P::dim, F_BODY, RIGHT);
      return;
    }
    if (mode == JOIN) {
      wl_status_t s = WiFi.status();
      sprite(S_SCOUT, ((int)(t * 6)) % 4, 104, 40, 2);
      if (s == WL_CONNECTED) {
        text("已連上 " + cfg.ssid, 120, 92, P::green, F_BODY, CENTER);
        text(WiFi.localIP().toString(), 120, 108, P::dim, F_SMALL, CENTER);
      } else if (millis() - joinAt > 15000 || s == WL_CONNECT_FAILED) {
        text("連不上，密碼可能錯了", 120, 92, P::red, F_BODY, CENTER);
        text("Enter 重新搜尋", 120, 108, P::dim, F_BODY, CENTER);
      } else text("連線中…", 120, 92, P::dim, F_BODY, CENTER);
      return;
    }
    const int vis = 7, rh = 15;
    keepVisible(sel, top, vis);
    if (!n) text("附近沒有找到 Wi-Fi，Enter 重新搜尋", 120, 70, P::dim, F_BODY, CENTER);
    for (int i = 0; i < vis && top + i < n; i++) {
      int k = top + i, y = 15 + i * rh;
      bool on = k == sel, curNet = WiFi.status() == WL_CONNECTED && WiFi.SSID() == ssid[k];
      if (on) R(2, y, 234, rh - 1, P::amberD);
      int bars = rssi[k] > -55 ? 4 : rssi[k] > -65 ? 3 : rssi[k] > -75 ? 2 : 1;
      for (int b = 0; b < 4; b++) R(8 + b * 3, y + 11 - b * 2, 2, 2 + b * 2, b < bars ? (on ? P::amber : P::text) : P::line);
      text(ssid[k], 26, y + 11, on ? P::amber : P::text);
      if (lock[k]) bitmap(IC_LOCK, 4, 196, y + 5, P::dim);
      if (curNet) text("已連線", 232, y + 11, P::green, F_BODY, RIGHT);
    }
    scrollbar(237, 15, 105, top, vis, n);
    text("Enter 連線 · Tab 重新搜尋", 232, 131, P::dim, F_BODY, RIGHT);
  }

  void key(const KeyEv& e) override {
    if (mode == SCAN) { if (e.k == K_BACK) go(A_SETTINGS); return; }
    if (mode == PASS) {
      if (e.k == K_BACK) { mode = LIST; return; }
      if (e.k == K_DEL && pw.length()) pw.remove(pw.length() - 1);
      if (e.k == K_CHAR && pw.length() < 63) pw += e.c;
      if (e.k == K_OK) join(ssid[sel], pw);
      return;
    }
    if (mode == JOIN) {
      if (e.k == K_OK && WiFi.status() != WL_CONNECTED) startScan();
      else go(A_SETTINGS);
      return;
    }
    if (e.k == K_UP && n) sel = (sel + n - 1) % n;
    if (e.k == K_DOWN && n) sel = (sel + 1) % n;
    if (e.k == K_TAB) startScan();
    if (e.k == K_OK) {
      if (!n) { startScan(); return; }
      if (!lock[sel]) join(ssid[sel], "");
      else { pw = ssid[sel] == cfg.ssid ? cfg.pass : ""; mode = PASS; }
    }
    if (e.k == K_BACK) go(A_SETTINGS);
  }
} wifiApp;
