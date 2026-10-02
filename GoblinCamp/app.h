// App 框架：每個 app 是一個狀態，go() 切換。狀態列、時間、Wi-Fi 共用
#pragma once

struct App {
  virtual void enter() {}
  virtual void leave() {}
  virtual void draw(float t, float dt) = 0;
  virtual void key(const KeyEv& e) = 0;
  virtual bool textMode() { return false; }
};

enum AppId : uint8_t { A_HOME, A_VOCAB, A_NEWS, A_DEFENSE, A_CLAUDE, A_REMOTE, A_CLOCK, A_SETTINGS, A_WIFI, A_COUNT };
App* apps[A_COUNT];
App* cur = nullptr;
AppId curId = A_HOME;
void go(AppId id) {
  if (cur) cur->leave();
  curId = id; cur = apps[id];
  cur->enter();
}

// ─── 時間（連上 Wi-Fi 後用 NTP 對時） ───
bool timeOk(tm& t) { return getLocalTime(&t, 0) && t.tm_year > 120; }
String hhmm() {
  tm t;
  return timeOk(t) ? two(t.tm_hour) + ":" + two(t.tm_min) : String("--:--");
}

// ─── Wi-Fi ───
bool ntpStarted = false;
void wifiConnect() {
  if (!cfg.ssid.length()) return;
  WiFi.mode(WIFI_STA);
  WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str());
}
void wifiTick() {
  if (!ntpStarted && WiFi.status() == WL_CONNECTED) {
    configTzTime("CST-8", "pool.ntp.org", "time.google.com");
    ntpStarted = true;
  }
}

// ─── 狀態列：主選單顯示時間，app 裡顯示「‹ 標題」 ───
void statusBar(const char* title = nullptr) {
  R(0, 0, W, 12, P::panel); R(0, 12, W, 1, P::line);
  if (title) {
    text(String("‹ ") + title, 4, 10, P::text);
    text(hhmm(), 198, 10, P::dim, F_SMALL, RIGHT);
  } else {
    text(hhmm(), 4, 10, P::text, F_BODYB);
    tm t;
    if (auto* a = usage.cur()) {   // 有 Claude 額度就顯示 5 小時用量，沒有就顯示日期
      uint16_t c = a->h5 >= 85 ? P::red : a->h5 >= 60 ? P::amber : P::green;
      text("5h", 46, 10, P::dim, F_SMALL);
      bar(60, 4, 26, 5, a->h5 / 100.f, c);
      text(String(a->h5) + "%", 90, 10, P::text, F_SMALL);
      int32_t left = (int32_t)(a->r5 - usage.now());
      if (left > 0) { bitmap(IC_CLOCK, 7, 116, 3, P::dim); text(String(left / 3600) + ":" + two(left % 3600 / 60), 125, 10, P::dim, F_SMALL); }
    } else if (timeOk(t)) text(String(t.tm_mon + 1) + "/" + t.tm_mday + " 週" + WEEK[t.tm_wday], 44, 10, P::dim);
  }
  // Wi-Fi 訊號
  int bars = 0;
  if (WiFi.status() == WL_CONNECTED) { int r = WiFi.RSSI(); bars = r > -60 ? 3 : r > -72 ? 2 : 1; }
  for (int i = 0; i < 3; i++) R(203 + i * 3, 8 - i * 2, 2, 2 + i * 2, i < bars ? P::text : P::line);
  // 電量
  static int lv = -1;
  static uint32_t lvAt = 0;
  if (!lvAt || millis() - lvAt > 5000) { lv = M5.Power.getBatteryLevel(); lvAt = millis(); }
  R(215, 3, 18, 7, P::dim); R(216, 4, 16, 5, P::panel); R(233, 5, 1, 3, P::dim);
  if (lv >= 0) R(217, 5, max(1, lv * 14 / 100), 3, lv < 20 ? P::red : P::green);
}

// ─── 共用的清單列（設定、Wi-Fi、新聞） ───
void scrollbar(int x, int y, int h, int top, int vis, int total) {
  if (total <= vis) return;
  int th = max(6, h * vis / total), ty = y + (h - th) * top / (total - vis);
  R(x + 1, y, 1, h, P::line); R(x, ty, 3, th, P::dim);
}
void keepVisible(int sel, int& top, int vis) {
  if (sel < top) top = sel;
  if (sel >= top + vis) top = sel - vis + 1;
}
