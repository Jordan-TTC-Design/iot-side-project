// 正在播放：Mac 上 Spotify 的歌名、歌手、進度，左邊是像素唱片機，整張唱片就是專輯封面。資料來自橋接程式 /np.txt
// ‹ › 上一首／下一首、Enter 暫停／播放、; . 音量
#pragma once

struct MusicApp : App {
  String state = "", title, artist, album, artId;
  int pos = 0, dur = 0, vol = 0;
  uint32_t fetchedAt = 0, lastPoll = 0;
  static constexpr int ART = 48;   // 封面 48×48，放大 2 倍
  uint16_t art[ART * ART];
  bool hasArt = false;

  bool playing() { return state == "playing"; }
  void parse(const String& body) {
    String f[8]; int n = 0, s = 0;
    String line = body; line.trim();
    while (n < 8) { int tab = line.indexOf('\t', s); f[n++] = line.substring(s, tab < 0 ? line.length() : tab); if (tab < 0) break; s = tab + 1; }
    state = f[0];
    if (n < 8) { title = ""; return; }
    title = f[1]; artist = f[2]; album = f[3];
    pos = f[4].toInt(); dur = f[5].toInt(); vol = f[6].toInt();
    fetchedAt = millis();
    if (f[7] != artId) { artId = f[7]; loadArt(); }
  }
  void loadArt() {
    hasArt = false;
    if (!bridge.discover()) return;
    HTTPClient http;
    http.setTimeout(4000);
    if (!http.begin("http://" + bridge.ip.toString() + ":" + bridge.port + "/np/art")) return;
    if (http.GET() == 200 && http.getSize() == (int)sizeof(art)) {
      WiFiClient* s = http.getStreamPtr();
      uint8_t* p = (uint8_t*)art; size_t got = 0; uint32_t t0 = millis();
      while (got < sizeof(art) && millis() - t0 < 4000) {
        int a = s->available();
        if (a > 0) got += s->readBytes(p + got, min((size_t)a, sizeof(art) - got)); else delay(1);
      }
      for (auto& v : art) v = (v >> 8) | (v << 8);   // 網路上是高位元組在前
      hasArt = got == sizeof(art);
    }
    http.end();
  }
  void poll() { lastPoll = millis(); String b = bridge.get("/np.txt", 2000); if (b.length()) parse(b); }
  void cmd(const char* c) { String b = bridge.get((String("/np/cmd?c=") + c).c_str(), 3000); if (b.length()) parse(b); }
  void enter() override { poll(); }

  float angle = 0, spin = 0, arm = 0;   // 唱片角度、轉速、唱臂（0 收起、1 放在唱片上）
  // 唱片機：整張唱片是專輯封面，慢慢轉（約 12 秒一圈）；沒有封面就是一張空白黑膠
  void turntable(float dt) {
    R(4, 18, 98, 100, P::panel2); R(4, 18, 98, 1, P::line); R(4, 117, 98, 1, P::ink);
    const int cx = 48, cy = 66, r = 37;
    spin += ((playing() ? 1.f : 0.f) - spin) * min(1.f, dt * 1.5f);
    angle += spin * dt * 0.52f;
    cv.fillCircle(cx, cy, r + 3, P::ink);   // 唱盤邊
    if (hasArt) {
      // 逐點反算回原圖：48×48 的封面撐滿整張唱片，保持像素感
      float c = cosf(angle), s = sinf(angle), k = (float)ART / (2 * r);
      for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++) {
          if (dx * dx + dy * dy > r * r) continue;
          int sx = (int)floorf((c * dx + s * dy) * k) + ART / 2, sy = (int)floorf((-s * dx + c * dy) * k) + ART / 2;
          if (sx < 0 || sy < 0 || sx >= ART || sy >= ART) continue;
          cv.drawPixel(cx + dx, cy + dy, art[sy * ART + sx]);
        }
    } else {
      cv.fillCircle(cx, cy, r, rgb(0x16121f));   // 空白唱片
    }
    cv.fillCircle(cx, cy, 3, P::panel2); cv.fillCircle(cx, cy, 1, P::dim);   // 軸心
    // 唱臂：播放時放到唱片上，停放時靠在唱片外
    arm += ((playing() ? 1.f : 0.f) - arm) * min(1.f, dt * 3);
    int px = 92, py = 26;
    float a = (88 + arm * 37) * DEG_TO_RAD;
    int ex = px + (int)(cosf(a) * 44), ey = py + (int)(sinf(a) * 44);
    cv.fillCircle(px, py, 5, P::dim); cv.fillCircle(px, py, 2, P::panel);
    cv.drawLine(px, py, ex, ey, P::text); cv.drawLine(px + 1, py, ex + 1, ey, P::text);
    R(ex - 3, ey - 2, 7, 5, P::text);
  }
  // 播放中的小等化器
  void eq(int x, int y, float t) {
    for (int i = 0; i < 5; i++) {
      int h = playing() ? 2 + (int)((sinf(t * (6 + i * 1.7f) + i) * 0.5f + 0.5f) * 8) : 2;
      R(x + i * 3, y - h, 2, h, P::green);
    }
  }

  void draw(float t, float dt) override {
    if (millis() - lastPoll > 1000) poll();
    R(0, 0, W, H, P::panel);
    statusBar("正在播放");
    turntable(dt);
    int x = 110;
    if (!title.length()) {
      const char* why = WiFi.status() != WL_CONNECTED ? "先連上 Wi-Fi" : !bridge.found() ? "找不到 Mac 橋接程式" : "Spotify 沒在播";
      text(why, x, 52, P::amber);
      text("在 Mac 上放首歌，", x, 70, P::dim);
      text("或按 Enter 播放", x, 86, P::dim);
      return;
    }
    // 歌名最多兩行、歌手、專輯
    String tl[2]; int n = wrapWords(title, 126, F_BODYB, tl, 2);
    if (n == 1 && textWidth(tl[0].c_str(), F_BODYB) > 126) n = wrapCJK(title, 126, F_BODYB, tl, 2);
    for (int i = 0; i < n; i++) text(tl[i], x, 32 + i * 14, P::text, F_BODYB);
    int y = 32 + n * 14 + 2;
    String ar[1]; wrapCJK(artist, 126, F_BODY, ar, 1); text(ar[0], x, y, P::amber); y += 14;
    String al[1]; wrapCJK(album, 126, F_BODY, al, 1); text(al[0], x, y, P::dim);
    // 進度
    int p = pos + (playing() ? (millis() - fetchedAt) / 1000 : 0);
    p = min(p, dur);
    bar(x, 98, 124, 3, dur ? (float)p / dur : 0, P::green);
    auto mmss = [](int s) { return String(s / 60) + ":" + two(s % 60); };
    text(mmss(p), x, 110, P::dim, F_SMALL);
    eq(x + 50, 111, t);
    text(mmss(dur), 234, 110, P::dim, F_SMALL, RIGHT);
    // 底列
    R(0, 122, W, 13, P::panel2);
    text(playing() ? "‹ 上一首  Enter 暫停  下一首 ›" : "‹ 上一首  Enter 播放  下一首 ›", 6, 132, P::dim);
    text("音量 " + String(vol), 234, 132, P::text, F_SMALL, RIGHT);
  }

  void key(const KeyEv& e) override {
    if (e.k == K_BACK) { go(A_HOME); return; }
    if (e.k == K_OK || e.k == K_SPACE) { cmd("play"); blip(990, 30); }
    if (e.k == K_RIGHT) { cmd("next"); blip(880, 30); }
    if (e.k == K_LEFT) { cmd("prev"); blip(880, 30); }
    if (e.k == K_UP) cmd("volup");
    if (e.k == K_DOWN) cmd("voldown");
  }
} musicApp;
