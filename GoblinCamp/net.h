// 跟 Mac 橋接程式（tools/bridge.py）講話：用 mDNS 找 _goblin._tcp，抓額度和新聞
#pragma once
#include <ESPmDNS.h>
#include <HTTPClient.h>

struct Bridge {
  IPAddress ip;
  uint16_t port = 0;
  bool mdnsUp = false;
  uint32_t lastLook = 0;

  bool found() { return port != 0; }
  // 找橋接程式（會卡 1～2 秒，所以 30 秒最多找一次）
  bool discover() {
    if (WiFi.status() != WL_CONNECTED) return false;
    if (found()) return true;
    if (lastLook && millis() - lastLook < 30000) return false;
    lastLook = millis();
    if (!mdnsUp) mdnsUp = MDNS.begin("goblincamp");
    int n = MDNS.queryService("goblin", "tcp");
    if (n <= 0) return false;
    ip = MDNS.address(0);
    port = MDNS.port(0);
    Serial.printf("找到橋接程式 %s:%u\n", ip.toString().c_str(), port);
    return true;
  }
  // GET 回傳 body；失敗回傳空字串，並在下次重新找橋接程式（Mac 的 IP 可能換了）
  String get(const char* path, int timeoutMs = 3000) {
    if (!discover()) return "";
    HTTPClient http;
    http.setTimeout(timeoutMs);
    String url = "http://" + ip.toString() + ":" + port + path;
    if (!http.begin(url)) return "";
    int code = http.GET();
    String body = code == 200 ? http.getString() : String("");
    http.end();
    if (code != 200) { port = 0; lastLook = 0; }
    return body;
  }
} bridge;

// ─── Claude 額度 ───
struct Account { String name; int h5, wk; uint32_t r5, rwk, updated; };
struct Usage {
  Account acct[4];
  int n = 0, sel = 0;
  uint32_t bridgeNow = 0, fetchedAt = 0, lastTry = 0;
  bool ok = false;

  // 現在時間（epoch 秒）：有 NTP 就用 NTP，否則用橋接程式給的時間往後推
  uint32_t now() {
    time_t t = time(nullptr);
    if (t > 1700000000) return t;
    return bridgeNow ? bridgeNow + (millis() - fetchedAt) / 1000 : 0;
  }
  void fetch() {
    lastTry = millis();
    String body = bridge.get("/usage.txt");
    if (!body.length()) return;
    int k = 0, start = 0;
    while (start < (int)body.length()) {
      int nl = body.indexOf('\n', start); if (nl < 0) nl = body.length();
      String line = body.substring(start, nl); start = nl + 1;
      String f[7]; int fn = 0, s = 0;
      while (fn < 7) { int tab = line.indexOf('\t', s); f[fn++] = line.substring(s, tab < 0 ? line.length() : tab); if (tab < 0) break; s = tab + 1; }
      if (f[0] == "now") { bridgeNow = f[1].toInt(); fetchedAt = millis(); }
      if (f[0] == "A" && fn == 7 && k < 4) acct[k++] = {f[1], (int)f[2].toInt(), (int)f[4].toInt(), (uint32_t)f[3].toInt(), (uint32_t)f[5].toInt(), (uint32_t)f[6].toInt()};
    }
    n = k; ok = true;
    if (sel >= n) sel = 0;
  }
  // 每分鐘抓一次
  void tick() {
    if (WiFi.status() != WL_CONNECTED) return;
    if (!lastTry || millis() - lastTry > 60000) fetch();
  }
  Account* cur() { return n ? &acct[sel] : nullptr; }
} usage;

String fmtLeft(int32_t s) {
  if (s <= 0) return "已重置";
  int d = s / 86400, h = s % 86400 / 3600, m = s % 3600 / 60;
  if (d) return String(d) + " 天 " + h + " 小時";
  return String(h) + ":" + two(m) + ":" + two(s % 60);
}
