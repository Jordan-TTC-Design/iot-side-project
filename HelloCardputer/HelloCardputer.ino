#include <M5Cardputer.h>

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);            // true = 一併初始化鍵盤
  M5Cardputer.Display.setRotation(1);      // 橫式 240 x 135
  M5Cardputer.Display.setTextSize(2);
  M5Cardputer.Display.fillScreen(TFT_BLACK);
  M5Cardputer.Display.drawString("Hello, Jordan", 10, 56);
  Serial.begin(115200);
  Serial.println("booted");                // 這就是你的 console.log
}

void loop() {
  M5Cardputer.update();                    // 每圈都要呼叫，否則按鍵/電源狀態不會更新
}
