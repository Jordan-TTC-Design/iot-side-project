# 02 · Sprite 雙緩衝

**按任意鍵切換兩種畫法，親眼看差別。**

## 燒錄

```bash
make flash SKETCH=SpriteDemo
```

開機是紅字 `DIRECT`（故意的錯誤示範）—— 橘球會**閃爍**。
按任一鍵變綠字 `SPRITE` —— 同一顆球變滑順。

## 差別只在一件事

```cpp
// ❌ DIRECT：兩次操作，使用者都看得到
d.fillScreen(TFT_BLACK);           // 螢幕真的變黑了
d.fillCircle(x, y, 10, ORANGE);    // 才畫上球
                                   // ← 眼睛看得到中間那個全黑的瞬間

// ✅ SPRITE：三次操作，前兩次在記憶體裡
canvas.fillSprite(TFT_BLACK);      // 記憶體變黑，螢幕沒動
canvas.fillCircle(x, y, 10, ...);  // 記憶體上有球了，螢幕還是沒動
canvas.pushSprite(0, 0);           // ← 只有這行碰硬體，一次覆蓋整個螢幕
```

`M5Canvas` 就是一塊 240×135 的記憶體。所有繪圖在那裡完成，`pushSprite()` 才把成品整批送出。
概念同 virtual DOM：先在記憶體算出最終樣子，再一次性套用。

## 三個發現

**fps 兩種模式差不多。** 瓶頸不在畫圖而在 SPI 傳輸 ——
**Sprite 解決的是視覺撕裂，不是速度。**

**狀態與畫面要分離。** `step()` 只更新資料、`drawXxx()` 只負責畫。
之後每個專案都用這個骨架。

**記憶體代價比想像小。** 全域只比第 01 課多 384 bytes（301732 → 301348），
因為 Sprite 的 32KB 是在 `setup()` 裡動態配置的，不算全域。
開機時序列埠會印出真實剩餘量。

## 可以動手改

| 改哪裡 | 會發生什麼 |
|---|---|
| `setColorDepth(8)` → `16` | 顏色變準，記憶體 32KB → 63KB |
| `now - lastFrame < 16` → `100` | 降到 10fps，DIRECT 的閃爍更明顯 |
| `fillCircle` → `fillRect(x, y, w, h, c)` | 球變方塊 |

## 實測數字

```
Sketch uses 518123 bytes (39%)
Global variables use 26332 bytes (8%), leaving 301348 bytes
```
