// 念單字、念句子：先找 SD 卡上的快取，沒有就向 Mac 橋接程式要（/tts），邊下載邊寫進 SD 卡，下次離線也能念
// 聲音格式：8-bit 無號、11025Hz、單聲道的原始 PCM（tools/bridge.py、tools/build_audio.py 產生）
// 沒有 PSRAM，所以從 SD 卡一次讀 4KB，三塊輪流排進喇叭（一塊在播、一塊排隊、一塊在讀）
#pragma once

struct Speech {
  static constexpr size_t CHUNK = 4096;
  static constexpr uint32_t RATE = 11025;
  static constexpr int CH = 1;     // 喇叭的聲道 0 留給嗶聲
  uint8_t bufs[3][CHUNK];
  int next = 0;
  File file;

  static String urlencode(const String& s) {
    String o;
    const char* hex = "0123456789ABCDEF";
    for (uint8_t c : s) {
      if (isalnum(c) || c == '-' || c == '.' || c == '_') o += (char)c;
      else { o += '%'; o += hex[c >> 4]; o += hex[c & 15]; }
    }
    return o;
  }
  static void mkdirs(const String& path) {
    int slash = path.lastIndexOf('/');
    for (int i = 1; i <= slash; i++) if (i == slash || path[i] == '/') SD.mkdir(path.substring(0, i));
  }
  // 從橋接程式下載，直接寫進 SD 卡
  bool download(const String& path, const String& textToSay, int wpm) {
    if (!bridge.discover()) return false;
    HTTPClient http;
    http.setTimeout(8000);
    String url = "http://" + bridge.ip.toString() + ":" + bridge.port + "/tts?r=" + wpm + "&t=" + urlencode(textToSay);
    if (!http.begin(url)) return false;
    bool ok = false;
    if (http.GET() == 200) {
      mkdirs(path);
      String tmp = path + ".part";
      File f = SD.open(tmp, FILE_WRITE);
      if (f) {
        WiFiClient* s = http.getStreamPtr();
        int total = http.getSize(), got = 0;
        uint32_t t0 = millis();
        while (http.connected() && (total < 0 || got < total) && millis() - t0 < 10000) {
          int a = s->available();
          if (a <= 0) { delay(1); continue; }
          int n = s->readBytes(bufs[0], min((size_t)a, CHUNK));
          f.write(bufs[0], n); got += n;
        }
        f.close();
        ok = got > 0 && (total < 0 || got == total);
        if (ok) { SD.remove(path); SD.rename(tmp, path); } else SD.remove(tmp);
      }
    }
    http.end();
    return ok;
  }

  void stop() {
    M5Cardputer.Speaker.stop(CH);
    if (file) file.close();
  }
  // 把下一塊排進喇叭；主迴圈每一輪呼叫
  void tick() {
    if (!file) return;
    while (M5Cardputer.Speaker.isPlaying(CH) < 2) {
      int n = file.read(bufs[next], CHUNK);
      if (n <= 0) { file.close(); return; }
      M5Cardputer.Speaker.playRaw(bufs[next], n, RATE, false, 1, CH, false);
      next = (next + 1) % 3;
    }
  }
  bool playing() { return file || M5Cardputer.Speaker.isPlaying(CH); }

  bool say(const String& path, const String& textToSay, int wpm = 165) {
    stop();
    if (!sdOk) { toast("發音要用 SD 卡"); return false; }
    if (!SD.exists(path)) {
      if (WiFi.status() != WL_CONNECTED) { toast("沒有發音檔，連上 Wi-Fi 才能向 Mac 要"); return false; }
      R(66, 56, 108, 22, P::ink); text("向 Mac 要發音…", 120, 71, P::amber, F_BODY, CENTER); cv.pushSprite(0, 0);
      if (!download(path, textToSay, wpm)) { toast("找不到 Mac 橋接程式"); return false; }
    }
    if (!cfg.vol) toast("音量是 0，到設定調大");
    file = SD.open(path);
    next = 0;
    tick();
    return true;
  }
} speech;

void sayWord(uint16_t idx, const String& w) { speech.say("/goblin/audio/w/" + String(idx) + ".raw", w, 140); }
void saySentence(uint16_t idx, const String& s) { speech.say("/goblin/audio/s/" + String(idx) + ".raw", s, 165); }
