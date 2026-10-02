// 真實天氣：Open-Meteo（免費、不用金鑰），Cardputer 直接用 Wi-Fi 抓，每 15 分鐘一次
// 營火場景依天氣變化：白天／晚上、雲、雨、雪、霧、打雷；冷了哥布林發抖、熱了冒汗
#pragma once
#include <WiFiClientSecure.h>

struct City { const char* name; float lat, lon; };
const City CITIES[] = {{"台北", 25.04f, 121.56f}, {"新北", 25.01f, 121.47f}, {"桃園", 24.99f, 121.30f},
                       {"新竹", 24.80f, 120.97f}, {"台中", 24.15f, 120.67f}, {"台南", 22.99f, 120.21f},
                       {"高雄", 22.63f, 120.30f}, {"宜蘭", 24.75f, 121.75f}, {"花蓮", 23.99f, 121.60f}};
constexpr int CITY_N = sizeof(CITIES) / sizeof(CITIES[0]);

enum Sky : uint8_t { SKY_CLEAR, SKY_CLOUDY, SKY_FOG, SKY_RAIN, SKY_SNOW, SKY_STORM };

struct Weather {
  bool ok = false;
  float temp = 0;
  int code = 0;          // WMO 天氣代碼
  bool isDay = false;
  uint32_t sunrise = 0, sunset = 0;   // 今天的日出日落（epoch）
  uint32_t lastTry = 0;
  bool forced = false;   // 除錯：序列埠 WX 指令指定天氣，就不再自動抓

  Sky sky() {
    if (!ok) return SKY_CLEAR;
    if (code >= 95) return SKY_STORM;
    if ((code >= 71 && code <= 77) || code == 85 || code == 86) return SKY_SNOW;
    if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) return SKY_RAIN;
    if (code == 45 || code == 48) return SKY_FOG;
    if (code >= 2) return SKY_CLOUDY;
    return SKY_CLEAR;
  }
  // 雨的強度 0～1
  float rain() {
    if (code == 51 || code == 61 || code == 80 || code == 56 || code == 66) return 0.3f;
    if (code == 53 || code == 63 || code == 81) return 0.6f;
    if (sky() == SKY_RAIN || sky() == SKY_STORM) return 1.f;
    return 0;
  }
  const char* desc() {
    switch (sky()) {
      case SKY_STORM: return "雷雨";
      case SKY_SNOW: return "下雪";
      case SKY_RAIN: return rain() < 0.5f ? "小雨" : rain() < 0.9f ? "下雨" : "大雨";
      case SKY_FOG: return "起霧";
      case SKY_CLOUDY: return code == 2 ? "多雲" : "陰天";
      default: return code == 1 ? "晴時多雲" : "晴";
    }
  }
  // 天亮程度 0（夜）～1（白天），日出日落前後 40 分鐘漸變
  float daylight(uint32_t now) {
    if (!ok || !sunrise || !now) return isDay ? 1 : 0;
    auto ramp = [](float x) { return constrain(x, 0.f, 1.f); };
    float up = ramp(((int32_t)(now - sunrise) + 1200) / 2400.f);
    float down = ramp(((int32_t)(sunset - now) + 1200) / 2400.f);
    return min(up, down);
  }

  // 抓 JSON 裡某個欄位的數字（回應很小，不值得裝 JSON 函式庫）
  static float num(const String& js, const char* key, int from = 0) {
    int i = js.indexOf(key, from);
    if (i < 0) return NAN;
    i = js.indexOf(':', i) + 1;
    int e = i;
    while (e < (int)js.length() && js[e] != ',' && js[e] != '}') e++;
    return js.substring(i, e).toFloat();
  }
  void fetch() {
    lastTry = millis();
    if (WiFi.status() != WL_CONNECTED) return;
    const City& c = CITIES[cfg.city % CITY_N];
    WiFiClientSecure tls;
    tls.setInsecure();   // 只是天氣，不驗證憑證；省下放根憑證的麻煩
    HTTPClient http;
    http.setTimeout(6000);
    String url = String("https://api.open-meteo.com/v1/forecast?latitude=") + String(c.lat, 2) + "&longitude=" + String(c.lon, 2) +
                 "&current=temperature_2m,weather_code,is_day&daily=sunrise,sunset&timeformat=unixtime&timezone=Asia%2FTaipei&forecast_days=1";
    if (!http.begin(tls, url)) return;
    if (http.GET() == 200) {
      String js = http.getString();
      int cur = js.indexOf("\"current\"");
      float t = num(js, "\"temperature_2m\"", cur), wc = num(js, "\"weather_code\"", cur), d = num(js, "\"is_day\"", cur);
      int daily = js.indexOf("\"daily\"");
      int sr = js.indexOf("\"sunrise\"", daily), ss = js.indexOf("\"sunset\"", daily);
      if (!isnan(t) && !isnan(wc)) {
        temp = t; code = (int)wc; isDay = d > 0.5f; ok = true;
        if (sr > 0) sunrise = js.substring(js.indexOf('[', sr) + 1).toInt();
        if (ss > 0) sunset = js.substring(js.indexOf('[', ss) + 1).toInt();
        Serial.printf("天氣：%s %.1f°C code %d\n", c.name, temp, code);
      }
    }
    http.end();
  }
  void tick() {
    if (forced || WiFi.status() != WL_CONNECTED) return;
    if (!lastTry || millis() - lastTry > (ok ? 15 * 60000UL : 60000UL)) fetch();
  }
  String line() { return ok ? String(CITIES[cfg.city % CITY_N].name) + " " + String((int)roundf(temp)) + "° " + desc() : String(""); }
} weather;

// 現在時間（epoch）：NTP 優先，否則用橋接程式給的時間
uint32_t epochNow() { time_t t = time(nullptr); return t > 1700000000 ? (uint32_t)t : usage.now(); }
