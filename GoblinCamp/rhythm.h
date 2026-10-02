// 音樂遊戲（合唱團的第 4 個模式）：音符從上面掉下來，掉到判定線時按標示的鍵，角色就唱那個音
// 曲子：內建 songs.h 的 32 首，加上 SD 卡 /goblin/songs/*.txt（自己的歌）
//   檔案格式：
//     # 曲名
//     bpm: 90
//     key: 0          （可省略：整首升降幾個半音）
//     1 1 5 5 | 6 6 5 - | ...   （簡譜，規則見 songs.h 開頭，可以寫好幾行）
#pragma once
#include "songs.h"

struct GNote { float t, dur; int8_t semi; uint8_t state; };   // t、dur：秒；semi：相對中央 Do 的半音；state 0 未判 1 打中 2 漏掉
enum Judge : uint8_t { J_NONE, J_PERFECT, J_GREAT, J_GOOD, J_MISS };

// 簡譜 → 音符。回傳音符數；拍子換成秒
int parseScore(const char* s, float bpm, int transpose, GNote* out, int maxN) {
  static const int8_t SEMI[8] = {0, 0, 2, 4, 5, 7, 9, 11};
  float beat = 60.f / bpm, t = 0;
  int n = 0;
  int last = -1;
  while (*s) {
    char c = *s;
    if (c == ' ' || c == '|' || c == '\n' || c == '\r' || c == '\t') { s++; continue; }
    if (c == '-') { if (last >= 0) out[last].dur += beat; t += beat; s++; continue; }
    int acc = 0;
    while (*s == '#' || *s == 'b') { acc += *s == '#' ? 1 : -1; s++; }
    if (*s < '0' || *s > '7') { s++; continue; }   // 看不懂的字就跳過
    int d = *s++ - '0';
    int oct = 0;
    while (*s == '\'' || *s == ',') { oct += *s == '\'' ? 1 : -1; s++; }
    float len = 1;
    while (*s == '_' || *s == '.') { len *= *s == '_' ? 0.5f : 1.5f; s++; }
    if (d == 0) { t += len * beat; last = -1; continue; }
    if (n < maxN) {
      out[n] = {t, len * beat, (int8_t)(SEMI[d] + acc + 12 * oct + transpose), 0};
      last = n++;
    }
    t += len * beat;
  }
  return n;
}

// 音高 → 要按的鍵（四排鍵＝四個八度），以及唱的角色
struct KeyLabel { char key; bool shift; int who; };
KeyLabel keyFor(int semi) {
  static const char* ROWS[4] = {"zxcvbnm", "asdfghj", "qwertyu", "1234567"};
  static const int8_t NAT[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};   // 半音 → 音名（升記號算在下面那個白鍵）
  static const bool SH[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
  int oct = (int)floorf(semi / 12.f), w = semi - oct * 12;
  int row = constrain(oct + 1, 0, 3);
  return {ROWS[row][NAT[w]], SH[w], NAT[w]};
}

struct Rhythm {
  enum Phase : uint8_t { SELECT, PLAY, RESULT } phase = SELECT;
  static constexpr int MAXN = 420;
  static constexpr float TRAVEL = 1.6f, LEAD = 2.0f;   // 音符從頂端掉到判定線幾秒；開始前空幾秒
  static constexpr int TOP = 30, HIT = 94;
  GNote notes[MAXN];
  int n = 0, sel = 0, top = 0;
  String sdTitles[16], sdPaths[16];
  int sdN = 0;
  bool demo = false;
  uint32_t startMs = 0;
  int score = 0, combo = 0, maxCombo = 0, cnt[5] = {0};
  Judge lastJ = J_NONE; uint32_t lastJAt = 0;
  float hitFlash[7] = {0};
  String title;
  float bpm = 100;

  int total() { return SONG_N + sdN; }
  String nameOf(int i) { return i < SONG_N ? String(SONGS[i].title) : sdTitles[i - SONG_N]; }
  String bestKey(int i) { return i < SONG_N ? "hs" + String(i) : "hsd" + String(i - SONG_N); }

  void scanSD() {
    sdN = 0;
    if (!sdOk) return;
    File d = SD.open("/goblin/songs");
    while (d && sdN < 16) {
      File f = d.openNextFile();
      if (!f) break;
      String nm = f.name();
      if (!f.isDirectory() && nm.endsWith(".txt")) {
        String t = nm.substring(0, nm.length() - 4);
        String first = f.readStringUntil('\n'); first.trim();
        if (first.startsWith("#")) { t = first.substring(1); t.trim(); }
        sdTitles[sdN] = t; sdPaths[sdN] = "/goblin/songs/" + nm; sdN++;
      }
      f.close();
    }
  }

  bool load(int i) {
    title = nameOf(i);
    if (i < SONG_N) { bpm = SONGS[i].bpm; n = parseScore(SONGS[i].score, bpm, 0, notes, MAXN); }
    else {
      File f = SD.open(sdPaths[i - SONG_N]);
      if (!f) return false;
      String score; int tr = 0; bpm = 100;
      while (f.available()) {
        String line = f.readStringUntil('\n'); line.trim();
        if (line.startsWith("#")) continue;
        if (line.startsWith("bpm:")) { bpm = line.substring(4).toFloat(); continue; }
        if (line.startsWith("key:")) { tr = line.substring(4).toInt(); continue; }
        score += line + " ";
      }
      f.close();
      if (bpm < 30) bpm = 100;
      n = parseScore(score.c_str(), bpm, tr, notes, MAXN);
    }
    // 超出四排鍵的範圍，就整首移八度
    int lo = 99, hi = -99;
    for (int k = 0; k < n; k++) { lo = min(lo, (int)notes[k].semi); hi = max(hi, (int)notes[k].semi); }
    int shift = 0;
    while (lo + shift < -12) shift += 12;
    while (hi + shift > 47 && lo + shift - 12 >= -12) shift -= 12;
    for (int k = 0; k < n; k++) notes[k].semi += shift;
    return n > 0;
  }

  void start(int i, bool demoMode) {
    if (!load(i)) { toast("這首讀不到"); return; }
    demo = demoMode; phase = PLAY; startMs = millis();
    score = combo = maxCombo = 0; for (auto& c : cnt) c = 0; lastJ = J_NONE;
  }
  float now() { return (millis() - startMs) / 1000.f - LEAD; }

  void sing(int semi) {
    KeyLabel k = keyFor(semi);
    choir.play(k.who, 0, semi - DEGREE_SEMI[k.who]);
    hitFlash[k.who] = 1;
  }
  void judge(GNote& g, Judge j) {
    g.state = j == J_MISS ? 2 : 1;
    cnt[j]++; lastJ = j; lastJAt = millis();
    if (j == J_MISS) { combo = 0; return; }
    combo++; maxCombo = max(maxCombo, combo);
    score += (j == J_PERFECT ? 300 : j == J_GREAT ? 200 : 100) + min(combo, 50) * 2;
  }
  void update() {
    if (phase != PLAY) return;
    float t = now();
    for (int k = 0; k < n; k++) {
      GNote& g = notes[k];
      if (g.state) continue;
      if (demo && t >= g.t) { sing(g.semi); judge(g, J_PERFECT); }
      else if (t > g.t + 0.2f) judge(g, J_MISS);
    }
    if (n && t > notes[n - 1].t + notes[n - 1].dur + 1.2f) finish();
  }
  void finish() {
    phase = RESULT;
    if (!demo) {
      int best = prefs.getInt(bestKey(sel).c_str(), 0);
      if (score > best) prefs.putInt(bestKey(sel).c_str(), score);
    }
  }
  float accuracy() { int tot = cnt[1] + cnt[2] + cnt[3] + cnt[4]; return tot ? (cnt[1] + cnt[2] * 0.7f + cnt[3] * 0.4f) / tot : 0; }
  const char* rank(float a) { return a >= 0.95f ? "S" : a >= 0.85f ? "A" : a >= 0.7f ? "B" : "C"; }

  // 玩家按了某個音
  void press(int semi) {
    if (phase != PLAY || demo) return;
    float t = now();
    GNote* best = nullptr; float bd = 1;
    for (int k = 0; k < n; k++) {
      GNote& g = notes[k];
      if (g.state || g.semi != semi) continue;
      float d = fabsf(t - g.t);
      if (d < bd) { bd = d; best = &g; }
      if (g.t > t + 0.3f) break;
    }
    sing(semi);
    if (best && bd <= 0.2f) judge(*best, bd < 0.07f ? J_PERFECT : bd < 0.13f ? J_GREAT : J_GOOD);
  }

  // ─── 畫面 ───
  void drawSelect() {
    R(0, 13, W, H - 13, P::panel);
    const int vis = 6, rh = 14, y0 = 34;
    keepVisible(sel, top, vis);
    for (int i = 0; i < vis && top + i < total(); i++) {
      int k = top + i, y = y0 + i * rh;
      bool on = k == sel;
      if (on) R(2, y, 234, rh - 1, P::amberD);
      String num = String(k + 1); if (k + 1 < 10) num = "0" + num;
      text(num, 8, y + 10, P::dim, F_SMALL);
      text(nameOf(k), 26, y + 11, on ? P::amber : P::text);
      if (k >= SONG_N) text("SD", 170, y + 10, P::blue, F_SMALL);
      int best = prefs.getInt(bestKey(k).c_str(), 0);
      if (best) text(String(best), 230, y + 10, P::dim, F_SMALL, RIGHT);
    }
    scrollbar(237, y0, vis * rh, top, vis, total());
    R(0, 122, W, 13, P::panel2);
    text("Enter 開始 · Space 示範 · ; . 選歌", 6, 132, P::dim);
  }

  void drawPlay(float tt) {
    float t = now();
    // 背景：舞台黑幕，7 條軌道
    R(0, 13, W, 85, rgb(0x12101e));
    for (int c = 0; c < 7; c++) {
      int x = 4 + c * 33;
      R(x + 1, TOP - 14, 30, HIT - TOP + 14, hitFlash[c] > 0 ? mix(rgb(0x12101e), rgb(0x4a3a20), hitFlash[c]) : rgb(0x18142a));
    }
    R(0, HIT, W, 2, P::amber);   // 判定線
    // 音符
    for (int k = 0; k < n; k++) {
      GNote& g = notes[k];
      float dtn = g.t - t;
      if (dtn > TRAVEL) break;
      if (g.state == 1) continue;
      if (dtn < -0.3f) continue;
      int y = HIT - (int)(dtn / TRAVEL * (HIT - TOP + 14)) - 5;
      KeyLabel kl = keyFor(g.semi);
      int x = 4 + kl.who * 33 + 2;
      uint16_t col = g.state == 2 ? P::line : SINGERS[kl.who].s == &S_QUEEN ? P::pink : kl.shift ? P::blue : P::amber;
      R(x, y, 28, 11, col);
      if (kl.shift) R(x + 2, y + 1, 24, 1, P::ink);   // 上面一條線＝要按 Shift
      char lab[2] = {kl.shift ? (char)toupper(kl.key) : kl.key, 0};
      text(lab, x + 14, y + 9, P::ink, F_BODYB, CENTER);
    }
    // 上方標題列蓋住剛出現的音符
    R(0, 13, W, 16, P::panel); R(0, 29, W, 1, P::line);
    // 判定字、連擊、分數
    static const char* JN[5] = {"", "Perfect", "Great", "Good", "Miss"};
    static const uint16_t JC[5] = {0, rgb(0xffe28a), P::green, P::blue, P::red};
    if (lastJ && millis() - lastJAt < 500) text(JN[lastJ], 120, 50, JC[lastJ], F_BODYB, CENTER);
    if (combo >= 3) text(String(combo) + " combo", 120, 64, P::text, F_SMALL, CENTER);
    text(String(score), 234, 26, P::amber, F_BODYB, RIGHT);
    text(demo ? "示範" : title.c_str(), 6, 26, P::dim, F_SMALL);
    if (t < 0) text(String((int)ceilf(-t)), 120, 60, P::text, F_BIG, CENTER);   // 開始前倒數
    // 進度
    if (n) bar(0, 13, W, 2, constrain(t / (notes[n - 1].t + 0.5f), 0.f, 1.f), P::green, rgb(0x12101e));
  }

  void drawResult() {
    R(0, 13, W, H - 13, P::panel);
    float a = accuracy();
    text(title, 120, 32, P::text, F_BODY, CENTER);
    text(rank(a), 46, 84, a >= 0.95f ? rgb(0xffe28a) : a >= 0.85f ? P::green : a >= 0.7f ? P::blue : P::red, F_CLOCK, CENTER);
    text(String(score) + " 分", 150, 52, P::amber, F_BODYB, CENTER);
    text("Perfect " + String(cnt[1]) + "  Great " + cnt[2], 150, 70, P::text, F_SMALL, CENTER);
    text("Good " + String(cnt[3]) + "  Miss " + cnt[4], 150, 84, P::text, F_SMALL, CENTER);
    text("最大連擊 " + String(maxCombo) + " · 準確 " + String((int)(a * 100)) + "%", 150, 100, P::dim, F_BODY, CENTER);
    R(0, 122, W, 13, P::panel2);
    text("Enter 再一次 · ` 回選歌", 6, 132, P::dim);
  }
} rhythm;
