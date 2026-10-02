// 今日新聞：讀 SD 卡的 /goblin/news.tsv（Mac 的 tools/build_news.py 產生）
// ‹ › 換文章、; . 選字、Space 念句子、Enter 加入單字卡、Tab 從 Mac 橋接程式更新
#pragma once

struct NewsApp : App {
  static constexpr int MAX_A = 8, MAX_W = 10;
  struct NWord { uint16_t idx; uint8_t lv; String w, kk, zh, s; };
  struct Article { String src, date, title; NWord words[MAX_W]; int n = 0; };
  Article arts[MAX_A];
  int na = 0, ai = 0, sel = 0, top = 0;
  bool loaded = false;
  const char* path = "/goblin/news.tsv";

  void load() {
    na = 0; loaded = true;
    File f = SD.open(path);
    if (!f) return;
    while (f.available() && na <= MAX_A) {
      String line = f.readStringUntil('\n');
      String c[7]; int n = 0, s = 0;
      while (n < 7) { int tab = line.indexOf('\t', s); c[n++] = line.substring(s, tab < 0 ? line.length() : tab); if (tab < 0) break; s = tab + 1; }
      if (c[0] == "A") { if (na == MAX_A) break; arts[na].src = c[1]; arts[na].date = c[2]; arts[na].title = c[3]; arts[na].n = 0; na++; }
      else if (c[0] == "W" && na && n == 7) {
        Article& a = arts[na - 1];
        if (a.n < MAX_W) a.words[a.n++] = {(uint16_t)c[1].toInt(), (uint8_t)c[2].toInt(), c[3], c[4], c[5], c[6]};
      }
    }
    f.close();
    ai = sel = top = 0;
  }
  void enter() override { if (!loaded) load(); }

  void update() {
    R(0, 0, W, H, P::panel);
    text("從 Mac 更新新聞…", 120, 70, P::amber, F_BODY, CENTER);
    cv.pushSprite(0, 0);
    String body = bridge.get("/news.tsv", 8000);
    if (!body.length()) { toast(WiFi.status() == WL_CONNECTED ? "找不到 Mac 橋接程式" : "先連上 Wi-Fi"); return; }
    SD.mkdir("/goblin");
    File f = SD.open(path, FILE_WRITE);
    if (f) { f.print(body); f.close(); }
    load();
    toast("新聞更新了：" + String(na) + " 篇");
  }

  void draw(float t, float) override {
    R(0, 0, W, H, P::panel);
    statusBar("今日新聞");
    if (!na) {
      sprite(S_SCOUT, ((int)(t * 2)) % 2, 104, 30, 2);
      text("還沒有新聞包", 120, 84, P::amber, F_BODY, CENTER);
      text("Tab 從 Mac 更新（要先跑 bridge.py）", 120, 104, P::dim, F_BODY, CENTER);
      return;
    }
    Article& a = arts[ai];
    text(a.src + " · " + a.date, 6, 25, P::blue, F_SMALL);
    text(String(ai + 1) + "/" + na + " 篇 ‹ ›", 234, 25, P::dim, F_BODY, RIGHT);
    String tl[2]; int tn = wrapWords(a.title, 228, F_BODYB, tl, 2);
    for (int i = 0; i < tn; i++) text(tl[i], 6, 39 + i * 12, P::text, F_BODYB);
    const int y0 = 39 + tn * 12 - 6, rh = 12, vis = 3;
    keepVisible(sel, top, vis);
    for (int i = 0; i < vis && top + i < a.n; i++) {
      int k = top + i, y = y0 + i * rh;
      NWord& w = a.words[k];
      bool on = k == sel;
      if (on) R(4, y, 232, rh - 1, P::amberD);
      R(7, y + 2, 15, 8, on ? P::amber : P::panel2);
      text("L" + String(w.lv), 14, y + 9, on ? P::ink : P::dim, F_SMALL, CENTER);
      text(w.w, 26, y + 9, on ? P::amber : P::text, F_BODYB);
      bool inDeck = vocab.ok && w.idx < vocab.count && vocab.box[w.idx] > 0;
      String zh[1]; wrapCJK(w.zh, 120, F_BODY, zh, 1);
      text((inDeck ? "已收 " : "") + zh[0], 232, y + 10, on ? P::text : P::dim, F_BODY, RIGHT);
    }
    // 原句
    NWord& w = a.words[sel];
    int py = y0 + vis * rh + 1;
    R(0, py, W, H - py, P::panel2);
    text("[" + w.kk + "]", 6, py + 11, P::blue);
    String lines[3]; int n = wrapWords(w.s, 228, F_BODY, lines, (H - py - 14) / 12);
    String key = w.w; key.toLowerCase();
    String stem = key.substring(0, max(3, (int)key.length() - 2));
    for (int i = 0; i < n; i++) {
      int x = 6, s = 0, y = py + 24 + i * 12;
      while (s <= (int)lines[i].length()) {
        int e = lines[i].indexOf(' ', s); if (e < 0) e = lines[i].length();
        String wd = lines[i].substring(s, e), low = wd; low.toLowerCase();
        bool hit = low.indexOf(stem) >= 0;
        x += text(wd + " ", x, y, hit ? P::amber : P::text, hit ? F_BODYB : F_BODY);
        s = e + 1;
      }
    }
  }

  void key(const KeyEv& e) override {
    if (e.k == K_BACK) { go(A_HOME); return; }
    if (e.k == K_TAB) { update(); return; }
    if (!na) return;
    Article& a = arts[ai];
    if (e.k == K_UP) { sel = (sel + a.n - 1) % a.n; blip(660); }
    if (e.k == K_DOWN) { sel = (sel + 1) % a.n; blip(660); }
    if (e.k == K_LEFT || e.k == K_RIGHT) { ai = (ai + (e.k == K_RIGHT ? 1 : na - 1)) % na; sel = top = 0; blip(740); }
    if (e.k == K_SPACE) {
      NWord& w = a.words[sel];
      uint32_t h = 5381;   // 新聞句子用內容算檔名
      for (char c : w.s) h = h * 33 + (uint8_t)c;
      speech.say("/goblin/audio/n/" + String(h, HEX) + ".raw", w.s, 165);
    }
    if (e.k == K_OK) {
      NWord& w = a.words[sel];
      if (!vocab.ok || w.idx >= vocab.count) { toast("沒有單字檔"); return; }
      if (vocab.box[w.idx] == 0) { vocab.box[w.idx] = 1; vocab.dirty = true; vocab.save(); toast("加入單字卡：" + w.w); }
      else toast(w.w + " 已經在單字卡裡");
      blip(990, 40);
    }
  }
} newsApp;
