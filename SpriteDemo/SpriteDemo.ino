// 第 02 課：Sprite 雙緩衝
// 按任意鍵可以在「直接畫螢幕」和「畫進 Sprite」之間切換，親眼看差別。
#include <M5Cardputer.h>

M5Canvas canvas(&M5Cardputer.Display);

bool  useSprite = false;      // 先從「錯誤示範」開始，才看得出對照
float ballX = 40, ballY = 60;
float velX  = 2.4f, velY = 1.7f;

uint32_t lastFrame = 0;
uint32_t frames = 0, fps = 0, lastFpsAt = 0;

void step() {                                  // 更新狀態（跟畫圖無關）
  ballX += velX;  ballY += velY;
  if (ballX < 12 || ballX > 228) velX = -velX;
  if (ballY < 12 || ballY > 123) velY = -velY;
}

void drawDirect() {                            // ❌ 直接畫在螢幕上 → 會閃
  auto &d = M5Cardputer.Display;
  d.fillScreen(TFT_BLACK);                     // 清空的瞬間使用者就看得到
  d.fillCircle((int)ballX, (int)ballY, 10, TFT_ORANGE);
  d.setTextColor(TFT_RED, TFT_BLACK);
  d.setTextSize(2);
  d.drawString("DIRECT", 8, 8);
  d.setTextSize(1);
  d.drawString(String(fps) + " fps", 8, 120);
}

void drawSprite() {                            // ✅ 畫在記憶體，一次推上去
  canvas.fillSprite(TFT_BLACK);
  canvas.fillCircle((int)ballX, (int)ballY, 10, TFT_ORANGE);
  canvas.setTextColor(TFT_GREEN);
  canvas.setTextSize(2);
  canvas.drawString("SPRITE", 8, 8);
  canvas.setTextSize(1);
  canvas.drawString(String(fps) + " fps", 8, 120);
  canvas.pushSprite(0, 0);                     // 這一行才真正碰到螢幕
}

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);

  canvas.setColorDepth(8);                     // 8-bit = 32KB；16-bit 要 63KB
  canvas.createSprite(240, 135);

  Serial.begin(115200);
  Serial.printf("sprite 配置後剩餘 heap: %u bytes\n", ESP.getFreeHeap());
}

void loop() {
  M5Cardputer.update();

  if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
    useSprite = !useSprite;
    M5Cardputer.Display.fillScreen(TFT_BLACK); // 切換時清一次，避免殘影
    Serial.printf("模式切換 -> %s\n", useSprite ? "SPRITE" : "DIRECT");
  }

  uint32_t now = millis();
  if (now - lastFrame < 16) return;            // 上限 ~60fps
  lastFrame = now;

  step();
  useSprite ? drawSprite() : drawDirect();

  frames++;                                    // 每秒統計一次 fps
  if (now - lastFpsAt >= 1000) { fps = frames; frames = 0; lastFpsAt = now; }
}
