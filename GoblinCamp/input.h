// 鍵盤：一般模式把 ; . , / ` 當方向鍵和返回；打字模式（輸入密碼、守城遊戲）原樣送出字元
#pragma once

enum Key : uint8_t { K_UP, K_DOWN, K_LEFT, K_RIGHT, K_OK, K_BACK, K_SPACE, K_DEL, K_TAB, K_CHAR };
struct KeyEv { Key k; char c; };

KeyEv evq[12];
int evn = 0;
uint32_t lastInput = 0;

void pushEv(Key k, char c = 0) { if (evn < 12) evq[evn++] = {k, c}; }

void pollKeys(bool textMode) {
  evn = 0;
  if (M5Cardputer.BtnA.wasPressed()) pushEv(K_BACK);   // 上方的 G0 鍵也當返回
  auto& kb = M5Cardputer.Keyboard;
  if (kb.isChange() && kb.isPressed()) {
    auto& st = kb.keysState();
    for (char c : st.word) {
      if (textMode) {
        if (c == '`') pushEv(K_BACK); else pushEv(K_CHAR, c);
        continue;
      }
      switch (c) {
        case ';': pushEv(K_UP); break;
        case '.': pushEv(K_DOWN); break;
        case ',': pushEv(K_LEFT); break;
        case '/': pushEv(K_RIGHT); break;
        case '`': pushEv(K_BACK); break;
        case ' ': pushEv(K_SPACE); break;
        default: pushEv(K_CHAR, c);
      }
    }
    if (st.del) pushEv(textMode ? K_DEL : K_BACK);
    if (st.tab) pushEv(K_TAB);
    if (st.enter) pushEv(K_OK);
  }
  if (evn) lastInput = millis();
}

// 嗶一聲。音量 0 就不叫
void blip(int freq = 880, int ms = 25);
