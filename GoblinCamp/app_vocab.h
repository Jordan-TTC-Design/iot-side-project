// 英語單字：單字卡＋Leitner 卡片盒。1 不會、2 模糊、3 會了；Space 翻面
#pragma once

struct VocabApp : App {
  static constexpr int DAILY = 12;
  uint16_t q[24];           // 這一輪的卡片（字的索引）
  int qn = 0, done = 0;
  bool flip = false;
  Word w;
  int loaded = -1;
  int sinceSave = 0;

  void build() {
    qn = 0; done = 0; flip = false; loaded = -1;
    if (!vocab.ok) return;
    int a = vocab.lvFirst[cfg.level], b = vocab.lvEnd[cfg.level];
    // 先複習學到一半的（盒子 1–4），再補新字，湊滿 DAILY 張
    for (int i = a; i < b && qn < DAILY; i++) if (vocab.box[i] >= 1 && vocab.box[i] <= 4) q[qn++] = i;
    for (int i = a; i < b && qn < DAILY; i++) if (vocab.box[i] == 0) q[qn++] = i;
  }
  void enter() override { build(); }
  void leave() override { vocab.save(); }

  void current() {
    if (qn && loaded != q[0]) { vocab.get(q[0], w); loaded = q[0]; }
  }
  // 把第一張卡往後插到 pos
  void requeue(int pos) {
    uint16_t c = q[0];
    memmove(q, q + 1, (qn - 1) * 2);
    pos = min(pos, qn - 1);
    memmove(q + pos + 1, q + pos, (qn - 1 - pos) * 2);
    q[pos] = c;
  }
  void rate(int r) {
    uint8_t& b = vocab.box[q[0]];
    if (r == 1) { b = 1; requeue(3); blip(440, 60); }
    if (r == 2) { b = max<uint8_t>(b, 1); requeue(7); blip(880, 40); }
    if (r == 3) {
      b = min(5, b + 1); done++; blip(1180, 50);
      memmove(q, q + 1, (qn - 1) * 2); qn--;
      // 這一級背熟 8 成就自動解鎖下一級（跳級測驗之後再做）
      if (cfg.unlocked == cfg.level && cfg.level < 6 && vocab.mastered(cfg.level) * 10 >= vocab.size(cfg.level) * 8) {
        cfg.unlocked++; saveSettings(); toast("解鎖第 " + String(cfg.unlocked) + " 級！");
      }
    }
    vocab.dirty = true; flip = false;
    if (++sinceSave >= 6) { vocab.save(); sinceSave = 0; }
  }

  void draw(float t, float) override {
    R(0, 0, W, H, P::panel);
    statusBar("英語單字");
    if (!vocab.ok) {
      text(sdOk ? "SD 卡上找不到單字檔" : "沒有讀到 SD 卡", 120, 56, P::amber, F_BODY, CENTER);
      text("把 sd/goblin/vocab.tsv 複製到", 120, 76, P::dim, F_BODY, CENTER);
      text("記憶卡的 /goblin/ 資料夾", 120, 92, P::dim, F_BODY, CENTER);
      return;
    }
    if (!qn) {
      sprite(S_SAGE, ((int)(t * 4)) % 4, 104, 34, 2);
      text(done ? "這一輪都會了！" : "這一級都背熟了", 120, 88, P::amber, F_BODY, CENTER);
      text("Enter 再來一輪 · ‹ › 換級數", 120, 106, P::dim, F_BODY, CENTER);
      return;
    }
    current();
    // 頂列：級數、詞性、進度
    R(6, 17, 22, 11, P::amber); text("Lv" + String(w.level), 17, 26, P::ink, F_SMALL, CENTER);
    text(w.pos + ".", 32, 26, P::dim, F_SMALL);
    text("這輪 " + String(done) + "/" + String(done + qn), 234, 26, P::dim, F_BODY, RIGHT);
    // 單字＋KK
    text(w.w, 8, 50, P::text, F_BIG);
    if (w.kk.length()) text("[" + w.kk + "]", 8, 66, P::blue, F_BODY);
    if (!flip) {
      text("Space 看答案", 120, 96, P::dim, F_BODY, CENTER);
      sprite(S_SAGE, 0, 198, 66 + (int)(sinf(t * 3) * 1.5f), 2);
      R(178, 54, 18, 11, P::panel2); text("?", 187, 63, P::amber, F_BODYB, CENTER);
    } else {
      String zh[1]; wrapCJK(w.zh, 226, F_BODY, zh, 1);
      text(zh[0], 8, 83, P::amber);
      String lines[2];
      int n = wrapWords(w.ex, 226, F_BODY, lines, 2);
      String key = w.w; key.toLowerCase();
      int y = 98;
      for (int i = 0; i < n; i++) {
        int x = 8, s = 0;
        while (s <= (int)lines[i].length()) {
          int e = lines[i].indexOf(' ', s); if (e < 0) e = lines[i].length();
          String wd = lines[i].substring(s, e), low = wd; low.toLowerCase();
          bool hit = key.length() >= 3 && low.indexOf(key.substring(0, max(3, (int)key.length() - 2))) >= 0;
          x += text(wd + " ", x, y, hit ? P::amber : P::text, hit ? F_BODYB : F_BODY);
          s = e + 1;
        }
        y += 12;
      }
      String zz[1]; wrapCJK(w.exZh, 226, F_BODY, zz, 1);
      text(zz[0], 8, y + 1, P::dim);
    }
    // 底列
    R(0, 122, W, 13, P::panel2);
    const char* lab[3] = {"不會", "模糊", "會了"};
    uint16_t col[3] = {P::red, P::amber, P::green};
    for (int i = 0; i < 3; i++) {
      int x = 8 + i * 58;
      R(x, 125, 9, 8, col[i]); text(String(i + 1), x + 5, 132, P::ink, F_SMALL, CENTER);
      text(lab[i], x + 12, 133, P::text);
    }
  }

  void key(const KeyEv& e) override {
    if (e.k == K_BACK) { go(A_HOME); return; }
    if (!vocab.ok) return;
    if (!qn) {
      if (e.k == K_OK) build();
      if (e.k == K_LEFT || e.k == K_RIGHT) changeLevel(e.k == K_RIGHT ? 1 : -1);
      return;
    }
    if (e.k == K_SPACE || e.k == K_OK) { flip = !flip; blip(740); }
    if (e.k == K_CHAR && e.c >= '1' && e.c <= '3') {
      if (!flip) { flip = true; return; }
      rate(e.c - '0');
    }
    if (e.k == K_LEFT || e.k == K_RIGHT) changeLevel(e.k == K_RIGHT ? 1 : -1);
  }
  void changeLevel(int d) {
    int lv = cfg.level + d;
    if (lv < 3 || lv > 6) return;
    if (lv > cfg.unlocked) { toast("第 " + String(lv) + " 級要先通過跳級測驗"); return; }
    vocab.save(); cfg.level = lv; saveSettings(); build();
    toast("切到第 " + String(lv) + " 級");
  }
} vocabApp;
