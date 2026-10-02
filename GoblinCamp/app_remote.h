// 遙控器：冷氣（日立／大金／國際牌／三菱，IRremoteESP8266 的 IRac）＋ LG 電視（NEC 碼）
// Cardputer ADV 的紅外線只能發射（GPIO 44），不能學習原本的遙控器，所以靠函式庫內建的協定。
// 同一個品牌有好幾種協定，「型號」一個一個試，冷氣有反應的那個就對了。
#pragma once
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRac.h>

constexpr uint8_t IR_PIN = 44;
IRac irAc(IR_PIN);
IRsend irTv(IR_PIN);

struct AcModel { decode_type_t proto; int16_t model; const char* name; };
const AcModel AC_HITACHI[] = {{HITACHI_AC424, -1, "AC424"}, {HITACHI_AC, -1, "AC"}, {HITACHI_AC1, -1, "AC1"},
                              {HITACHI_AC344, -1, "AC344"}, {HITACHI_AC264, -1, "AC264"}, {HITACHI_AC296, -1, "AC296"}};
const AcModel AC_DAIKIN[] = {{DAIKIN, -1, "標準"}, {DAIKIN2, -1, "2"}, {DAIKIN216, -1, "216"}, {DAIKIN160, -1, "160"},
                             {DAIKIN176, -1, "176"}, {DAIKIN128, -1, "128"}, {DAIKIN152, -1, "152"}, {DAIKIN64, -1, "64"},
                             {DAIKIN200, -1, "200"}, {DAIKIN312, -1, "312"}};
const AcModel AC_PANASONIC[] = {{PANASONIC_AC, kPanasonicJke, "JKE"}, {PANASONIC_AC, kPanasonicLke, "LKE"},
                                {PANASONIC_AC, kPanasonicNke, "NKE"}, {PANASONIC_AC, kPanasonicDke, "DKE"},
                                {PANASONIC_AC, kPanasonicCkp, "CKP"}, {PANASONIC_AC, kPanasonicRkr, "RKR"},
                                {PANASONIC_AC32, -1, "AC32"}};
const AcModel AC_MITSUBISHI[] = {{MITSUBISHI_AC, -1, "AC"}, {MITSUBISHI112, -1, "112"}, {MITSUBISHI136, -1, "136"},
                                 {MITSUBISHI_HEAVY_88, -1, "重工 88"}, {MITSUBISHI_HEAVY_152, -1, "重工 152"}};
struct AcBrand { const AcModel* models; uint8_t n; };
const AcBrand AC_BRANDS[4] = {{AC_HITACHI, 6}, {AC_DAIKIN, 10}, {AC_PANASONIC, 7}, {AC_MITSUBISHI, 5}};

const char* const MODES[5] = {"冷氣", "除濕", "送風", "暖氣", "自動"};
const char* const FANS[4] = {"自動", "低", "中", "高"};
const stdAc::opmode_t MODE_V[5] = {stdAc::opmode_t::kCool, stdAc::opmode_t::kDry, stdAc::opmode_t::kFan,
                                   stdAc::opmode_t::kHeat, stdAc::opmode_t::kAuto};
const stdAc::fanspeed_t FAN_V[4] = {stdAc::fanspeed_t::kAuto, stdAc::fanspeed_t::kLow, stdAc::fanspeed_t::kMedium,
                                    stdAc::fanspeed_t::kHigh};

// LG 電視（NEC 32-bit，位址 0x20DF）。key：Cardputer 上按的鍵
struct TvKey { char key; const char* label; uint32_t code; };
const TvKey TV_KEYS[] = {
    {'p', "電源", 0x20DF10EF}, {'m', "靜音", 0x20DF906F}, {'i', "輸入源", 0x20DFD02F}, {'h', "Home", 0x20DFC23D},
    {'=', "音量+", 0x20DF40BF}, {'-', "音量-", 0x20DFC03F}, {']', "頻道+", 0x20DF00FF}, {'[', "頻道-", 0x20DF807F},
    {'b', "返回", 0x20DF14EB},
};
// 方向鍵和 OK 直接對應電視
const uint32_t TV_UP = 0x20DF02FD, TV_DOWN = 0x20DF827D, TV_LEFT = 0x20DFE01F, TV_RIGHT = 0x20DF609F, TV_OK = 0x20DF22DD;

struct RemoteApp : App {
  uint8_t tab = 0;          // 0 冷氣 1 電視
  int row = 0;              // 冷氣頁目前選哪一列
  float ir = 0;             // 紅外線發射的動畫
  String lastTv;
  uint32_t lastTvAt = 0;
  bool begun = false;

  bool textMode() override { return tab == 1; }   // 電視頁要收字母快捷鍵
  void enter() override {
    if (!begun) { irTv.begin(); begun = true; }
  }

  const AcModel& model() { auto& b = AC_BRANDS[cfg.acBrand]; return b.models[cfg.acModel % b.n]; }

  void sendAc() {
    auto& m = model();
    irAc.next.protocol = m.proto;
    irAc.next.model = m.model;
    irAc.next.power = cfg.acPower;
    irAc.next.mode = MODE_V[cfg.acMode];
    irAc.next.celsius = true;
    irAc.next.degrees = cfg.acTemp;
    irAc.next.fanspeed = FAN_V[cfg.acFan];
    irAc.next.swingv = cfg.acSwing ? stdAc::swingv_t::kAuto : stdAc::swingv_t::kOff;
    irAc.next.swingh = stdAc::swingh_t::kOff;
    irAc.sendAc();
    ir = 1; blip(1800, 20);
    saveSettings();
  }
  void sendTv(uint32_t code, const char* label) {
    irTv.sendNEC(code, 32);
    ir = 1; blip(1800, 20);
    lastTv = label; lastTvAt = millis();
  }

  void tabs() {
    const char* names[2] = {"冷氣", "LG 電視"};
    int x = 6;
    for (int i = 0; i < 2; i++) {
      int w = textWidth(names[i]) + 12;
      bool on = i == tab;
      R(x, 17, w, 14, on ? P::amber : P::panel2);
      text(names[i], x + w / 2, 28, on ? P::ink : P::dim, F_BODY, CENTER);
      x += w + 4;
    }
    text("Tab 切換", 214, 28, P::dim, F_BODY, RIGHT);
    // 紅外線 LED 和發射的波紋
    R(222, 20, 4, 7, P::pink);
    if (ir > 0) for (int i = 0; i < 3; i++) cv.drawArc(226, 23, 4 + i * 4 + (int)((1 - ir) * 4), 3 + i * 4 + (int)((1 - ir) * 4), -50, 50, mix(P::panel, P::pink, ir));
  }

  void draw(float t, float dt) override {
    R(0, 0, W, H, P::panel);
    statusBar("遙控器");
    if (ir > 0) ir -= dt * 2.5f;
    tabs();
    if (tab == 0) drawAc(); else drawTv();
  }

  void drawAc() {
    // 左邊大字
    R(6, 36, 88, 84, P::panel2);
    if (cfg.acPower) text(String(cfg.acTemp), 44, 78, cfg.acMode == 3 ? P::red : P::blue, F_CLOCK, CENTER);
    else text("關機", 50, 74, P::line, F_BODY, CENTER);
    if (cfg.acPower) text("°C", 80, 64, P::dim, F_BODY);
    text(cfg.acPower ? String(MODES[cfg.acMode]) + " · 風" + FANS[cfg.acFan] : String("Enter 開機"), 50, 96, P::dim, F_BODY, CENTER);
    text(String(BRANDS[cfg.acBrand]) + " " + model().name, 50, 112, P::dim, F_BODY, CENTER);
    // 右邊各列
    String vals[7] = {BRANDS[cfg.acBrand],
                      String(model().name) + "（" + (cfg.acModel % AC_BRANDS[cfg.acBrand].n + 1) + "/" + AC_BRANDS[cfg.acBrand].n + "）",
                      cfg.acPower ? "開" : "關", String(cfg.acTemp) + " °C", MODES[cfg.acMode], FANS[cfg.acFan], cfg.acSwing ? "開" : "關"};
    const char* labels[7] = {"品牌", "型號", "電源", "溫度", "模式", "風速", "擺風"};
    for (int i = 0; i < 7; i++) {
      int y = 35 + i * 14;
      bool on = i == row;
      if (on) R(100, y, 136, 14, P::amberD);
      text(labels[i], 105, y + 11, on ? P::amber : P::dim);
      text(on ? "‹ " + vals[i] + " ›" : vals[i], 232, y + 11, P::text, F_BODY, RIGHT);
    }
  }

  void drawTv() {
    // 按鍵對照表：兩欄
    const int n = sizeof(TV_KEYS) / sizeof(TV_KEYS[0]);
    for (int i = 0; i < n; i++) {
      int x = 8 + (i % 2) * 74, y = 36 + (i / 2) * 15;
      bool hot = lastTvAt && millis() - lastTvAt < 400 && lastTv == TV_KEYS[i].label;
      R(x, y, 14, 12, hot ? P::amber : P::panel2);
      text(String(TV_KEYS[i].key), x + 7, y + 10, hot ? P::ink : P::amber, F_BODYB, CENTER);
      text(TV_KEYS[i].label, x + 18, y + 10, P::text);
    }
    // 方向鍵示意
    int cx = 196, cy = 66;
    auto pad = [&](int dx, int dy, const char* s, const char* label) {
      bool hot = lastTvAt && millis() - lastTvAt < 400 && lastTv == label;
      R(cx + dx - 9, cy + dy - 8, 18, 16, hot ? P::amber : P::panel2);
      text(s, cx + dx, cy + dy + 5, hot ? P::ink : P::text, F_BODYB, CENTER);
    };
    pad(0, -18, ";", "上"); pad(0, 18, ".", "下"); pad(-20, 0, ",", "左"); pad(20, 0, "/", "右"); pad(0, 0, "OK", "OK");
    text("Enter = OK", cx, 104, P::dim, F_SMALL, CENTER);
    if (lastTvAt && millis() - lastTvAt < 1200) text("送出 " + lastTv, 232, 132, P::pink, F_BODY, RIGHT);
    else text("` 離開", 232, 132, P::dim, F_BODY, RIGHT);
  }

  void key(const KeyEv& e) override {
    if (e.k == K_TAB) { tab ^= 1; blip(740); return; }
    if (e.k == K_BACK) { go(A_HOME); return; }
    if (tab == 1) {
      if (e.k == K_OK) { sendTv(TV_OK, "OK"); return; }
      if (e.k == K_DEL) { sendTv(0x20DF14EB, "返回"); return; }
      if (e.k != K_CHAR) return;
      switch (e.c) {   // 文字模式下方向鍵是原本的字元
        case ';': sendTv(TV_UP, "上"); return;
        case '.': sendTv(TV_DOWN, "下"); return;
        case ',': sendTv(TV_LEFT, "左"); return;
        case '/': sendTv(TV_RIGHT, "右"); return;
      }
      for (auto& k : TV_KEYS) if (k.key == tolower(e.c)) { sendTv(k.code, k.label); return; }
      return;
    }
    if (e.k == K_UP) { row = (row + 6) % 7; blip(660); return; }
    if (e.k == K_DOWN) { row = (row + 1) % 7; blip(660); return; }
    int d = e.k == K_RIGHT ? 1 : e.k == K_LEFT ? -1 : 0;
    if (e.k == K_OK) {
      if (row <= 1) { sendAc(); toast("送出測試訊號"); return; }   // 在品牌／型號列按 Enter 就送一次試試
      if (row == 2 || !cfg.acPower) { cfg.acPower = !cfg.acPower; sendAc(); return; }
      d = 1;
    }
    if (!d) return;
    auto& b = AC_BRANDS[cfg.acBrand];
    switch (row) {
      case 0: cfg.acBrand = (cfg.acBrand + d + 4) % 4; cfg.acModel = 0; saveSettings(); return;
      case 1: cfg.acModel = (cfg.acModel + d + b.n) % b.n; saveSettings(); toast(String("型號 ") + model().name + "，Enter 試送看冷氣有沒有反應"); return;
      case 2: cfg.acPower = !cfg.acPower; break;
      case 3: cfg.acTemp = constrain(cfg.acTemp + d, 16, 30); break;
      case 4: cfg.acMode = (cfg.acMode + d + 5) % 5; break;
      case 5: cfg.acFan = (cfg.acFan + d + 4) % 4; break;
      case 6: cfg.acSwing = !cfg.acSwing; break;
    }
    if (cfg.acPower || row == 2) sendAc(); else saveSettings();
  }
} remoteApp;
