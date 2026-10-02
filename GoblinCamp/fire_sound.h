// 營火白噪音：在機器上即時合成，不用音檔。布朗噪音當底（呼呼的燃燒聲）＋隨機的劈啪爆裂聲
// 每次劈啪都會通知營火動畫噴一撮火星（fireBurst）
#pragma once

volatile int fireBurst = 0;   // 營火動畫每畫一次就消耗掉，噴出火星

struct FireSound {
  static constexpr uint32_t RATE = 16000;
  static constexpr int N = 1024;      // 一塊 64ms，三塊輪流
  static constexpr int CH = 2;        // 喇叭聲道：0 嗶聲、1 發音、2 營火
  int16_t bufs[3][N];
  int next = 0;
  bool on = false;
  float brown = 0, hiss = 0, swell = 0, swellT = 0;
  int popLeft = 0;                    // 目前這聲劈啪還剩幾個取樣
  float popAmp = 0, popDecay = 0;
  uint32_t rng = 1;

  float rnd() { rng = rng * 1664525u + 1013904223u; return (int32_t)rng / 2147483648.f; }   // -1～1

  void fill(int16_t* b) {
    float gain = cfg.fireVol / 10.f;
    for (int i = 0; i < N; i++) {
      float w = rnd();
      // 底：布朗噪音（低沉）＋一點點高頻沙沙聲，音量隨一條很慢的曲線起伏
      brown = (brown + 0.02f * w) / 1.02f;
      hiss = hiss * 0.7f + w * 0.3f;
      swellT += 1.f / RATE;
      swell = 0.75f + 0.25f * sinf(swellT * 0.7f) * sinf(swellT * 0.23f + 1);
      float s = brown * 3.2f * swell + hiss * 0.025f;
      // 劈啪：平均每秒 2～3 聲，三成機率連續好幾下
      if (popLeft == 0 && (rng >> 8) % 6400 == 0) {
        popLeft = 40 + (rng >> 4) % 400;           // 2.5～28ms
        popAmp = 0.35f + 0.6f * fabsf(rnd());
        popDecay = 0.985f + 0.012f * fabsf(rnd());
        fireBurst++;
      }
      if (popLeft > 0) {
        s += rnd() * popAmp;
        popAmp *= popDecay;
        if (--popLeft == 0 && fabsf(rnd()) < 0.3f) popLeft = -(int)(200 + (rng >> 6) % 1200);   // 短暫停頓後再一聲
      } else if (popLeft < 0 && ++popLeft == 0) {
        popLeft = 20 + (rng >> 5) % 200; popAmp = 0.2f + 0.4f * fabsf(rnd()); popDecay = 0.98f; fireBurst++;
      }
      b[i] = (int16_t)constrain(s * gain * 12000.f, -32000.f, 32000.f);
    }
  }
  void tick() {
    if (!on) return;
    while (M5Cardputer.Speaker.isPlaying(CH) < 2) {
      fill(bufs[next]);
      M5Cardputer.Speaker.playRaw(bufs[next], N, RATE, false, 1, CH, false);
      next = (next + 1) % 3;
    }
  }
  void toggle() {
    on = !on;
    if (!on) M5Cardputer.Speaker.stop(CH);
    else { rng = esp_random() | 1; tick(); }
  }
} fireSound;
