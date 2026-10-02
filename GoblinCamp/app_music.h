// 正在播放：Mac 上 Spotify 的歌名、歌手、進度，左邊一台像素唱片機。資料來自橋接程式 /np.txt
// ‹ › 上一首／下一首、Enter 暫停／播放、; . 音量
#pragma once

struct MusicApp : App {
  String state = "", title, artist, album, artId;
  int pos = 0, dur = 0, vol = 0;
  uint32_t fetchedAt = 0, lastPoll = 0;
  uint16_t art[32 * 32];
  bool hasArt = false;
  float angle = 0, arm = 0;   // 唱片角度、唱臂位置（0 收起、1 放在唱片上）

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

  void turntable(float t, float dt) {
    // 底座
    R(4, 18, 98, 100, P::panel2); R(4, 18, 98, 1, P::line); R(4, 117, 98, 1, P::ink);
    int cx = 48, cy = 66;
    // 轉速 33⅓：播放時轉、暫停時慢慢停下
    static float spin = 0;
    spin += ((playing() ? 1.f : 0.f) - spin) * min(1.f, dt * 2);
    angle += spin * dt * 3.5f;
    cv.fillCircle(cx, cy, 40, P::ink);
    cv.fillCircle(cx, cy, 38, rgb(0x16121f));
    for (int r = 16; r < 38; r += 3) cv.drawCircle(cx, cy, r, rgb(0x231d33));   // 紋路
    // 反光：兩道跟著轉
    for (int k = 0; k < 2; k++) {
      float a = angle + k * PI;
      for (int r = 18; r < 37; r++) cv.drawPixel(cx + (int)(cosf(a) * r), cy + (int)(sinf(a) * r), rgb(0x3a3150));
    }
    // 中間的標籤：專輯封面縮成 24×24
    if (hasArt) {
      for (int j = 0; j < 24; j++) for (int i = 0; i < 24; i++) {
        int dx = i - 12, dy = j - 12;
        if (dx * dx + dy * dy > 144) continue;   // 圓形標籤
        cv.drawPixel(cx - 12 + i, cy - 12 + j, art[(j * 32 / 24) * 32 + (i * 32 / 24)]);
      }
    } else cv.fillCircle(cx, cy, 12, P::amber);
    cv.fillCircle(cx, cy, 2, P::panel2);
    // 唱臂：播放時放下
    arm += ((playing() ? 1.f : 0.f) - arm) * min(1.f, dt * 3);
    int px = 92, py = 26;
    float a = (88 + arm * 37) * DEG_TO_RAD;   // 停放時直直靠在唱片外，播放時轉進來
    int ex = px + (int)(cosf(a) * 44), ey = py + (int)(sinf(a) * 44);
    cv.fillCircle(px, py, 5, P::dim); cv.fillCircle(px, py, 2, P::panel);
    cv.drawLine(px, py, ex, ey, P::text); cv.drawLine(px + 1, py, ex + 1, ey, P::text);
    R(ex - 3, ey - 2, 7, 5, P::text);
  }

  void draw(float t, float dt) override {
    if (millis() - lastPoll > 1000) poll();
    R(0, 0, W, H, P::panel);
    statusBar("正在播放");
    turntable(t, dt);
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
