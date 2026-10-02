// Claude 額度：5 小時、每週用量和重置倒數；多帳號用 ‹ › 切換。資料來自 Mac 橋接程式
#pragma once

uint16_t pctColor(int p) { return p >= 85 ? P::red : p >= 60 ? P::amber : P::green; }

struct ClaudeApp : App {
  void enter() override { if (WiFi.status() == WL_CONNECTED) usage.fetch(); }

  void block(int y, const char* label, int pct, const String& reset) {
    text(label, 6, y, P::dim);
    text(String(pct) + "%", 234, y + 2, pctColor(pct), F_BODYB, RIGHT);
    bar(6, y + 5, 228, 8, pct / 100.f, pctColor(pct));
    for (int i = 1; i < 10; i++) R(6 + 228 * i / 10, y + 5, 1, 8, P::panel);
    bitmap(IC_CLOCK, 7, 6, y + 18, P::dim);
    text(reset, 16, y + 25, P::text);
  }

  void draw(float t, float) override {
    R(0, 0, W, H, P::panel);
    statusBar("Claude 額度");
    Account* a = usage.cur();
    if (!a) {
      const char* why = WiFi.status() != WL_CONNECTED ? "先到設定連上 Wi-Fi"
                        : !bridge.found()             ? "找不到 Mac 橋接程式"
                                                      : "Mac 上還沒有額度資料";
      sprite(S_WORKER, ((int)(t * 2)) % 2, 104, 30, 2);
      text(why, 120, 84, P::amber, F_BODY, CENTER);
      text("Mac：python3 tools/bridge.py", 120, 102, P::dim, F_SMALL, CENTER);
      text("Enter 重新抓取", 120, 120, P::dim, F_BODY, CENTER);
      return;
    }
    // 帳號分頁
    int x = 6;
    for (int i = 0; i < usage.n; i++) {
      int w = textWidth(usage.acct[i].name.c_str()) + 12;
      bool on = i == usage.sel;
      R(x, 17, w, 14, on ? P::amber : P::panel2);
      text(usage.acct[i].name, x + w / 2, 28, on ? P::ink : P::dim, F_BODY, CENTER);
      x += w + 4;
    }
    if (usage.n > 1) text("‹ › 切換", 234, 28, P::dim, F_BODY, RIGHT);
    uint32_t now = usage.now();
    block(46, "5 小時", a->h5, fmtLeft((int32_t)(a->r5 - now)) + " 後重置");
    time_t rw = a->rwk; tm r; localtime_r(&rw, &r);
    block(86, "每週", a->wk, "週" + String(WEEK[r.tm_wday]) + " " + two(r.tm_hour) + ":" + two(r.tm_min) + " · 還有 " + fmtLeft((int32_t)(a->rwk - now)));
    R(0, 122, W, 13, P::panel2);
    int ago = now > a->updated ? (now - a->updated) / 60 : 0;
    text(ago < 1 ? String("剛剛更新") : ago < 120 ? String(ago) + " 分鐘前更新" : String(ago / 60) + " 小時前更新", 6, 132, P::dim);
    // 5 小時快用完時，哥布林累倒
    if (a->h5 >= 85) { sprite(S_WORKER, 0, 212, 108, 1, LYING); text("z", 228, 106 - ((int)(t * 4)) % 6, P::blue, F_SMALL); }
    else sprite(S_WORKER, ((int)(t * 2)) % 2, 216, 106, 1);
  }

  void key(const KeyEv& e) override {
    if (e.k == K_BACK) { go(A_HOME); return; }
    if ((e.k == K_LEFT || e.k == K_RIGHT) && usage.n > 1) { usage.sel = (usage.sel + (e.k == K_RIGHT ? 1 : usage.n - 1)) % usage.n; blip(740); }
    if (e.k == K_OK) { bridge.lastLook = 0; usage.fetch(); toast(usage.n ? "已更新" : "還是抓不到"); }
  }
} claudeApp;
