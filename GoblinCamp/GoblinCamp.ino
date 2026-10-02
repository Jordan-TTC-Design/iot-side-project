// 哥布林營地 Cardputer：主選單＋營地動畫，右邊一條一條的 app
// 計畫見 PLAN.md；介面原型見 docs/launcher-prototype.html
#include <M5Cardputer.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <Preferences.h>
#include <time.h>

#include "assets_gen.h"   // 由 tools/make_assets.py 產生
#include "ui.h"
#include "input.h"
#include "store.h"
#include "fire_sound.h"
#include "net.h"
#include "audio.h"
#include "weather.h"
#include "camp.h"
#include "app.h"
#include "app_home.h"
#include "app_vocab.h"
#include "app_defense.h"
#include "app_settings.h"
#include "app_remote.h"
#include "app_claude.h"
#include "app_news.h"
#include "app_music.h"
#include "app_choir.h"
#include "serial_sd.h"

void blip(int freq, int ms) {
  if (cfg.vol) M5Cardputer.Speaker.tone(freq, ms);
}

bool screenOff = false;

void setup() {
  Serial.setRxBufferSize(8192);   // 預設 256 bytes，USB 傳檔時會丟資料；要在 begin 之前設
  Serial.begin(115200);
  auto c = M5.config();
  M5Cardputer.begin(c, true);
  M5Cardputer.Display.setRotation(1);

  cv.setColorDepth(16);
  cv.createSprite(W, H);
  cv.setTextDatum(lgfx::baseline_left);
  fontsBegin();
  campInit();
  randomSeed(esp_random());

  setenv("TZ", "CST-8", 1);   // 開機就用台灣時間；RTC 在 reset 後還留著上次對時的時間
  tzset();
  loadSettings();
  applySettings();
  sdBegin();
  uint32_t t0 = millis();
  vocab.load();
  Serial.printf("SD %s，單字 %d 個（%lu ms）\n", sdOk ? "OK" : "沒有", vocab.count, millis() - t0);
  wifiConnect();

  apps[A_HOME] = &homeApp;     apps[A_VOCAB] = &vocabApp;   apps[A_NEWS] = &newsApp;
  apps[A_DEFENSE] = &defenseApp; apps[A_CLAUDE] = &claudeApp; apps[A_MUSIC] = &musicApp; apps[A_CHOIR] = &choirApp; apps[A_REMOTE] = &remoteApp;
  apps[A_CLOCK] = &clockApp;   apps[A_SETTINGS] = &settingsApp; apps[A_WIFI] = &wifiApp;
  go(A_HOME);
  lastInput = millis();
}

void loop() {
  M5Cardputer.update();
  wifiTick();
  serialTick();
  usage.tick();
  speech.tick();
  fireSound.tick();   // 關螢幕時營火聲也繼續
  weather.tick();
  choir.tick();
  pollKeys(cur->textMode());

  // 關螢幕時，第一個按鍵只負責叫醒
  if (screenOff) {
    if (evn) { screenOff = false; applySettings(); }
    return;
  }
  for (int i = 0; i < evn; i++) cur->key(evq[i]);

  uint32_t idle = millis() - lastInput;
  if (curId == A_HOME && cfg.idleClock && idle > 45000) go(A_CLOCK);
  uint32_t off = SLEEP_MS[cfg.sleep];
  if (curId == A_CLOCK && cfg.clockAwake) off = 0;   // 營火時鐘可以設成常亮
  if (off && idle > off) {
    screenOff = true;
    vocab.save();
    M5Cardputer.Display.setBrightness(0);
    return;
  }

  static uint32_t last = millis();
  uint32_t now = millis();
  if (now - last < 33) return;   // 約 30fps
  float dt = min(0.1f, (now - last) / 1000.f);
  last = now;
  cur->draw(now / 1000.f, dt);
  drawToast();
  cv.pushSprite(0, 0);
}
