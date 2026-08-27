// 第 03 課：鍵盤輸入框
// 打字、退格、Enter 送出。TextInput 這個 struct 之後可以直接複製去用。
#include <M5Cardputer.h>

M5Canvas canvas(&M5Cardputer.Display);

// ─────────────────────────────────────────────
// 可重複使用的輸入框元件
// ─────────────────────────────────────────────
struct TextInput {
  String value;
  size_t maxLen = 34;          // 240px 寬、6px 字寬，扣掉提示符大約放得下
  bool   changed = false;      // 這一輪內容有沒有變（用來決定要不要重畫）

  // 回傳 true 代表使用者按了 Enter
  bool poll() {
    changed = false;
    if (!M5Cardputer.Keyboard.isChange())  return false;
    if (!M5Cardputer.Keyboard.isPressed()) return false;

    auto st = M5Cardputer.Keyboard.keysState();

    for (auto c : st.word) {                    // 一般可見字元（含空白）
      if (value.length() < maxLen) { value += c; changed = true; }
    }
    if (st.del && value.length()) {             // 退格
      value.remove(value.length() - 1);
      changed = true;
    }
    if (st.tab && value.length()) {              // Tab = 整行清空
      value = "";
      changed = true;
    }
    if (st.enter) return true;                  // 送出
    return false;
  }

  String take() { String s = value; value = ""; changed = true; return s; }
};

// ─────────────────────────────────────────────
TextInput input;

const int  HIST_MAX = 6;
String     history[HIST_MAX];
int        histCount = 0;

void pushHistory(const String &s) {
  if (histCount < HIST_MAX) {
    history[histCount++] = s;
  } else {
    for (int i = 0; i < HIST_MAX - 1; i++) history[i] = history[i + 1];
    history[HIST_MAX - 1] = s;
  }
}

void render() {
  canvas.fillSprite(TFT_BLACK);

  canvas.fillRect(0, 0, 240, 15, TFT_ORANGE);   // 標題列
  canvas.setTextColor(TFT_BLACK);
  canvas.setTextSize(1);
  canvas.drawString("TEXT INPUT", 6, 4);
  canvas.drawString(String(input.value.length()) + "/" + String(input.maxLen), 190, 4);

  canvas.setTextColor(TFT_WHITE);               // 送出過的內容
  for (int i = 0; i < histCount; i++) {
    canvas.drawString(history[i], 8, 24 + i * 11);
  }

  canvas.drawRect(4, 114, 232, 17, TFT_DARKGREY);   // 輸入列
  canvas.setTextColor(TFT_GREEN);
  canvas.drawString("> " + input.value, 8, 119);

  if ((millis() / 500) % 2) {                   // 每 500ms 閃一次的游標
    int x = 8 + (2 + input.value.length()) * 6;
    canvas.fillRect(x, 118, 6, 10, TFT_GREEN);
  }

  canvas.pushSprite(0, 0);
}

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);          // 第二個參數 true 才會啟用鍵盤
  M5Cardputer.Display.setRotation(1);
  canvas.setColorDepth(8);
  canvas.createSprite(240, 135);
  Serial.begin(115200);
  pushHistory("打字看看，Enter 送出");
}

void loop() {
  M5Cardputer.update();

  if (input.poll()) {                    // 按了 Enter
    String line = input.take();
    if (line.length()) {
      pushHistory(line);
      Serial.printf("送出: %s\n", line.c_str());
    }
  }


  static uint32_t last = 0;
  if (millis() - last >= 33) { last = millis(); render(); }   // 30fps（游標要閃）
}
