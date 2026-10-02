// 設定（存在 NVS）、SD 卡、單字資料（SD 卡 /goblin/vocab.tsv）與背誦進度（/goblin/progress.bin）
#pragma once

// ─── 設定 ───
struct Settings {
  uint8_t vol = 6;        // 0–10
  uint8_t bright = 7;     // 1–10
  uint8_t sleep = 1;      // 自動關螢幕：0=30 秒 1=1 分 2=5 分 3=永不
  bool idleClock = true;  // 主選單閒置 45 秒切到營火時鐘
  bool clockAwake = false; // 營火時鐘畫面不自動關螢幕（在時鐘按 Space 切換）
  uint8_t level = 3;      // 目前背的級數（3–6）
  uint8_t unlocked = 3;   // 已解鎖到第幾級；往上要通過跳級測驗
  uint8_t acBrand = 1;    // 冷氣品牌：0 日立 1 大金 2 國際牌 3 三菱
  uint8_t acModel = 0;    // 同品牌的第幾種協定（試到有反應的那個）
  bool acPower = false;
  uint8_t acTemp = 26, acMode = 0, acFan = 0;  // 模式：冷氣 除濕 送風 暖氣 自動；風速：自動 低 中 高
  bool acSwing = true;
  String ssid, pass;
} cfg;
Preferences prefs;
const uint32_t SLEEP_MS[4] = {30000, 60000, 300000, 0};

void loadSettings() {
  prefs.begin("camp", false);
  cfg.vol = prefs.getUChar("vol", cfg.vol);
  cfg.bright = prefs.getUChar("bright", cfg.bright);
  cfg.sleep = prefs.getUChar("sleep", cfg.sleep);
  cfg.idleClock = prefs.getBool("idleClock", cfg.idleClock);
  cfg.clockAwake = prefs.getBool("clockAwake", cfg.clockAwake);
  cfg.level = prefs.getUChar("level", cfg.level);
  cfg.unlocked = prefs.getUChar("unlocked", cfg.unlocked);
  cfg.acBrand = prefs.getUChar("acBrand", cfg.acBrand);
  cfg.acModel = prefs.getUChar("acModel", cfg.acModel);
  cfg.acPower = prefs.getBool("acPower", cfg.acPower);
  cfg.acTemp = prefs.getUChar("acTemp", cfg.acTemp);
  cfg.acMode = prefs.getUChar("acMode", cfg.acMode);
  cfg.acFan = prefs.getUChar("acFan", cfg.acFan);
  cfg.acSwing = prefs.getBool("acSwing", cfg.acSwing);
  cfg.ssid = prefs.getString("ssid", "");
  cfg.pass = prefs.getString("pass", "");
}
void saveSettings() {
  prefs.putUChar("vol", cfg.vol);
  prefs.putUChar("bright", cfg.bright);
  prefs.putUChar("sleep", cfg.sleep);
  prefs.putBool("idleClock", cfg.idleClock);
  prefs.putBool("clockAwake", cfg.clockAwake);
  prefs.putUChar("level", cfg.level);
  prefs.putUChar("unlocked", cfg.unlocked);
  prefs.putUChar("acBrand", cfg.acBrand);
  prefs.putUChar("acModel", cfg.acModel);
  prefs.putBool("acPower", cfg.acPower);
  prefs.putUChar("acTemp", cfg.acTemp);
  prefs.putUChar("acMode", cfg.acMode);
  prefs.putUChar("acFan", cfg.acFan);
  prefs.putBool("acSwing", cfg.acSwing);
  prefs.putString("ssid", cfg.ssid);
  prefs.putString("pass", cfg.pass);
}
void applySettings() {
  M5Cardputer.Display.setBrightness(20 + (cfg.bright - 1) * 26);
  M5Cardputer.Speaker.setVolume(cfg.vol * 25);
}

// ─── SD 卡 ───
bool sdOk = false;
void sdBegin() {
  SPI.begin(40, 39, 14, 12);
  sdOk = SD.begin(12, SPI, 25000000);
}

// ─── 單字 ───
struct Word { uint8_t level = 0; String w, kk, pos, zh, ex, exZh; };

struct Vocab {
  const char* path = "/goblin/vocab.tsv";
  const char* progPath = "/goblin/progress.bin";
  bool ok = false;
  uint16_t count = 0;
  uint32_t* off = nullptr;      // 每一行在檔案裡的位置
  uint8_t* box = nullptr;       // Leitner 卡片盒：0=新字 1–4=學習中 5=熟了
  uint16_t lvFirst[8] = {0}, lvEnd[8] = {0};
  bool dirty = false;

  // 掃一遍檔案記下每行開頭。6000 行約 0.5 秒
  bool load() {
    ok = false;
    if (!sdOk) return false;
    File f = SD.open(path);
    if (!f) return false;
    count = 0;
    size_t cap = 6400;
    off = (uint32_t*)realloc(off, cap * 4);
    static uint8_t buf[2048];
    uint32_t pos = 0; bool lineStart = true; int lastLv = 0;
    for (int i = 0; i < 8; i++) lvFirst[i] = lvEnd[i] = 0;
    while (true) {
      int n = f.read(buf, sizeof(buf));
      if (n <= 0) break;
      for (int i = 0; i < n; i++, pos++) {
        if (lineStart && count < cap) {
          int lv = buf[i] - '0';
          if (lv >= 1 && lv <= 6) {
            if (lv != lastLv) { lvFirst[lv] = count; lastLv = lv; }
            lvEnd[lv] = count + 1;
            off[count++] = pos;
          }
        }
        lineStart = buf[i] == '\n';
      }
    }
    f.close();
    box = (uint8_t*)realloc(box, count);
    memset(box, 0, count);
    File p = SD.open(progPath);
    if (p) { p.read(box, min((size_t)p.size(), (size_t)count)); p.close(); }
    ok = count > 0;
    return ok;
  }

  bool get(uint16_t i, Word& w) {
    if (!ok || i >= count) return false;
    File f = SD.open(path);
    if (!f) return false;
    f.seek(off[i]);
    String line = f.readStringUntil('\n');
    f.close();
    String* fields[7] = {nullptr, &w.w, &w.kk, &w.pos, &w.zh, &w.ex, &w.exZh};
    int start = 0;
    for (int k = 0; k < 7; k++) {
      int tab = line.indexOf('\t', start);
      String v = line.substring(start, tab < 0 ? line.length() : tab);
      if (k == 0) w.level = v.toInt(); else *fields[k] = v;
      if (tab < 0) break;
      start = tab + 1;
    }
    return true;
  }

  void save() {
    if (!ok || !dirty) return;
    SD.mkdir("/goblin");
    File p = SD.open(progPath, FILE_WRITE);
    if (p) { p.write(box, count); p.close(); dirty = false; }
  }

  int mastered(int lv) { int n = 0; for (int i = lvFirst[lv]; i < lvEnd[lv]; i++) n += box[i] >= 5; return n; }
  int size(int lv) { return lvEnd[lv] - lvFirst[lv]; }
} vocab;
