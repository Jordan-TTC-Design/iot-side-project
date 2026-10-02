// 哥布林合唱團：7 個角色各唱一個音。三種模式（Tab 切換）：
//   唱名：機器上合成，每個角色音色不同（啊、喔、咿…）唱 Do Re Mi Fa Sol La Si
//   錄音：Enter 再按任一個琴鍵，倒數後錄那個角色 1.5 秒，存在 SD 卡 /goblin/choir/rec/N.raw
//   自訂：SD 卡 /goblin/choir/custom/N.wav（16kHz 單聲道 16-bit，用 tools/choir_put.py 轉檔上傳）
// 鍵盤當鋼琴：四排鍵＝四個八度（z 排最低、a 排中音、q 排高音、數字排最高），每排從左邊 Do 開始，
// 往右超過 Si 就接著上一個八度；按住 Shift 升半音。誰唱照音名決定（所有的 Do 都是平民唱）
// 錄音和自訂音檔會依音高自動變調（改變播放速度），一段聲音就能彈整個音階。最多 4 個音同時發聲
#pragma once
#include <functional>

struct Singer { const Sheet* s; const char* name; float f1, f2; };   // f1 f2：嗓音的共振峰（決定像哪個母音）
const Singer SINGERS[7] = {
    {&S_WORKER, "平民", 700, 1220},   // 啊
    {&S_SCOUT, "敏捷", 300, 2300},    // 咿
    {&S_SAGE, "聰明", 450, 800},      // 喔
    {&S_BRUTE, "壯碩", 350, 900},     // 嗚（低沉）
    {&S_GOLDEN, "金皮", 600, 1700},   // 欸
    {&S_HALF, "混血", 400, 1000},     // 喔嗚
    {&S_QUEEN, "公主", 650, 1500},    // 啦
};
const char* const SOLFEGE[7] = {"Do", "Re", "Mi", "Fa", "Sol", "La", "Si"};
const int8_t DEGREE_SEMI[7] = {0, 2, 4, 5, 7, 9, 11};

struct Choir {
  static constexpr uint32_t RATE = 16000;
  static constexpr int N = 512;          // 一塊 32ms
  static constexpr int CH = 3;           // 喇叭聲道：0 嗶聲、1 發音、2 營火、3 合唱團
  static constexpr int TBL = 256;
  int16_t table[7][TBL];                 // 每個角色一個週期的波形
  int16_t out[3][N];
  int next = 0;

  struct Voice {
    bool on = false;
    int who = 0;
    bool synth = true;
    float phase = 0, inc = 0, env = 0, age = 0, len = 0.7f;
    File f;
    uint32_t dataEnd = 0;
    int16_t buf[N]; int bufN = 0; float pos = 0, ratio = 1;
  } v[4];

  // 加法合成：諧波強度依共振峰決定，聽起來像不同母音
  void build() {
    for (int c = 0; c < 7; c++) {
      float f0 = 220;
      float amp[16];
      for (int h = 1; h <= 16; h++) {
        float hf = f0 * h;
        float a1 = expf(-sq((hf - SINGERS[c].f1) / 180.f)), a2 = expf(-sq((hf - SINGERS[c].f2) / 260.f));
        amp[h - 1] = (a1 + 0.6f * a2 + 0.35f / h) / h * 2;
      }
      float peak = 0, tmp[TBL];
      for (int i = 0; i < TBL; i++) {
        float s = 0;
        for (int h = 1; h <= 16; h++) s += amp[h - 1] * sinf(2 * PI * h * i / TBL);
        tmp[i] = s; peak = max(peak, fabsf(s));
      }
      for (int i = 0; i < TBL; i++) table[c][i] = (int16_t)(tmp[i] / peak * 30000);
    }
  }

  // 讀 WAV 檔頭，找到 data 區段；只收 16-bit 單聲道
  static bool openWav(File& f, uint32_t& end, uint32_t& rate) {
    uint8_t h[12];
    if (f.read(h, 12) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) return false;
    uint16_t fmt = 0, chn = 0, bits = 0; rate = 0;
    while (f.available()) {
      uint8_t ck[8]; if (f.read(ck, 8) != 8) return false;
      uint32_t sz = ck[4] | ck[5] << 8 | ck[6] << 16 | ck[7] << 24;
      if (!memcmp(ck, "fmt ", 4)) {
        uint8_t b[16]; f.read(b, 16);
        fmt = b[0] | b[1] << 8; chn = b[2] | b[3] << 8; rate = b[4] | b[5] << 8 | b[6] << 16 | b[7] << 24; bits = b[14] | b[15] << 8;
        if (sz > 16) f.seek(f.position() + sz - 16);
      } else if (!memcmp(ck, "data", 4)) {
        end = f.position() + sz;
        return fmt == 1 && chn == 1 && bits == 16;
      } else f.seek(f.position() + sz + (sz & 1));
    }
    return false;
  }

  String recPath(int who) { return "/goblin/choir/rec/" + String(who + 1) + ".raw"; }
  String customPath(int who) { return "/goblin/choir/custom/" + String(who + 1) + ".wav"; }

  // mode：0 唱名 1 錄音 2 自訂；semis：相對於角色自己那個音差幾個半音
  bool play(int who, int mode, int semis) {
    Voice* p = nullptr;
    for (auto& x : v) if (!x.on) { p = &x; break; }
    if (!p) { p = &v[0]; for (auto& x : v) if (x.age > p->age) p = &x; }   // 都在響就搶最久的那個
    if (p->f) p->f.close();
    p->on = true; p->who = who; p->age = 0; p->env = 0; p->pos = 0; p->bufN = 0;
    float shift = powf(2, semis / 12.f);
    if (mode == 0) {
      p->synth = true;
      float freq = 261.63f * powf(2, (DEGREE_SEMI[who] + semis) / 12.f);
      p->inc = freq * TBL / RATE; p->phase = 0; p->len = 0.7f;
      return true;
    }
    p->synth = false;
    if (!sdOk) { p->on = false; return false; }
    String path = mode == 1 ? recPath(who) : customPath(who);
    p->f = SD.open(path);
    if (!p->f) { p->on = false; return false; }
    uint32_t rate = RATE;
    if (mode == 2) {
      if (!openWav(p->f, p->dataEnd, rate)) { p->f.close(); p->on = false; toast("音檔格式不對，用 choir_put.py 轉"); return false; }
    } else p->dataEnd = p->f.size();
    p->ratio = shift * rate / RATE;
    return true;
  }

  void fill(int16_t* o) {
    int32_t acc[N] = {0};
    for (auto& x : v) {
      if (!x.on) continue;
      for (int i = 0; i < N; i++) {
        float s;
        if (x.synth) {
          // 起音 30ms、持續、尾音 250ms；0.15 秒後加一點顫音
          float a = x.age < 0.03f ? x.age / 0.03f : x.age < x.len ? 1.f : max(0.f, 1 - (x.age - x.len) / 0.25f);
          float vib = x.age > 0.15f ? 1 + 0.006f * sinf(2 * PI * 5.5f * x.age) : 1;
          s = table[x.who][(int)x.phase & (TBL - 1)] * a;
          x.phase += x.inc * vib;
          if (x.age > x.len + 0.25f) { x.on = false; break; }
        } else {
          int ip = (int)x.pos;
          while (ip >= x.bufN) {   // 這一塊用完了，從 SD 卡再讀一塊
            x.pos -= x.bufN; ip = (int)x.pos;
            int want = min((int)sizeof(x.buf), (int)(x.dataEnd - x.f.position()));
            x.bufN = want > 0 ? x.f.read((uint8_t*)x.buf, want) / 2 : 0;
            if (x.bufN <= 0) break;
          }
          if (x.bufN <= 0) { x.on = false; x.f.close(); break; }
          s = x.buf[ip] * (x.age < 0.01f ? x.age / 0.01f : 1.f);
          x.pos += x.ratio;
        }
        x.age += 1.f / RATE;
        acc[i] += (int32_t)(s * 0.45f);
      }
    }
    for (int i = 0; i < N; i++) o[i] = (int16_t)constrain(acc[i], -32000, 32000);
  }
  bool active() { for (auto& x : v) if (x.on) return true; return false; }
  void tick() {
    if (!active()) return;
    while (M5Cardputer.Speaker.isPlaying(CH) < 2) {
      fill(out[next]);
      M5Cardputer.Speaker.playRaw(out[next], N, RATE, false, 1, CH, false);
      next = (next + 1) % 3;
      if (!active()) break;
    }
  }
  bool singing(int who) { for (auto& x : v) if (x.on && x.who == who) return true; return false; }

  // 錄 1.5 秒：喇叭和麥克風不能同時用，先關喇叭；錄完把音量拉到滿
  bool record(int who, std::function<void(float)> meter) {
    if (!sdOk) return false;
    for (auto& x : v) { x.on = false; if (x.f) x.f.close(); }
    String path = recPath(who);
    Speech::mkdirs(path);
    File f = SD.open(path, FILE_WRITE);
    if (!f) return false;
    M5Cardputer.Speaker.end();
    M5Cardputer.Mic.begin();
    const int CHUNK = 1600;            // 0.1 秒
    static int16_t buf[2][1600];
    int32_t peak = 1;
    for (int k = 0; k < 15; k++) {
      int16_t* b = buf[k & 1];
      M5Cardputer.Mic.record(b, CHUNK, RATE);
      while (M5Cardputer.Mic.isRecording()) delay(1);
      int32_t pk = 0;
      for (int i = 0; i < CHUNK; i++) pk = max(pk, (int32_t)abs(b[i]));
      peak = max(peak, pk);
      f.write((uint8_t*)b, CHUNK * 2);
      meter(pk / 32768.f);
    }
    f.close();
    M5Cardputer.Mic.end();
    M5Cardputer.Speaker.begin();
    applySettings();
    // 正規化：讀回來放大到峰值約 90%
    float g = min(8.f, 29000.f / peak);
    f = SD.open(path, "r+");
    if (f) {
      for (uint32_t off = 0; off < f.size(); off += CHUNK * 2) {
        f.seek(off);
        int n = f.read((uint8_t*)buf[0], CHUNK * 2) / 2;
        for (int i = 0; i < n; i++) buf[0][i] = (int16_t)constrain(buf[0][i] * g, -32000.f, 32000.f);
        f.seek(off); f.write((uint8_t*)buf[0], n * 2);
      }
      f.close();
    }
    return true;
  }
} choir;

struct ChoirApp : App {
  int mode = 0;
  bool armRec = false;
  float hop[7] = {0};
  bool has[2][7];   // 錄音／自訂音檔是否存在

  bool textMode() override { return true; }   // 每個字母鍵都是琴鍵
  void scan() {
    for (int i = 0; i < 7; i++) { has[0][i] = sdOk && SD.exists(choir.recPath(i)); has[1][i] = sdOk && SD.exists(choir.customPath(i)); }
  }
  void enter() override {
    static bool built = false;
    if (!built) { choir.build(); built = true; }
    scan();
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

  void draw(float t, float dt) override {
    stage(t, dt);
    statusBar("哥布林合唱團");
    const char* names[3] = {"唱名", "錄音", "自訂"};
    int x = 6;
    for (int i = 0; i < 3; i++) {
      int w = textWidth(names[i]) + 10;
      R(x, 16, w, 13, i == mode ? P::amber : P::panel2);
      text(names[i], x + w / 2, 26, i == mode ? P::ink : P::dim, F_BODY, CENTER);
      x += w + 3;
    }
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
    if (e.k == K_BACK) { go(A_HOME); return; }
    if (e.k == K_TAB) { mode = (mode + 1) % 3; armRec = false; scan(); blip(740); return; }
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
