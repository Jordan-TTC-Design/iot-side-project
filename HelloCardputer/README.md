# 01 · Hello, Cardputer

第一支能燒進機器的程式。目標只有一個：確認環境與硬體都通了。

## 燒錄

```bash
make flash SKETCH=HelloCardputer
```

燒完把機身側邊電源開關撥到 **ON**，螢幕出現 `Hello, Cardputer`。

## 程式在做什麼

Arduino 沒有 `main()`，框架幫你寫好了，等同：

```js
setup()                    // 開機跑一次
while (true) { loop() }    // 之後無限重複
```

| 那一行 | 在做什麼 |
|---|---|
| `#include <M5Cardputer.h>` | 等同 import，但是**文字層級的複製貼上**。這也是為什麼這支小程式編出來有 511KB —— 螢幕驅動、字型、WiFi 堆疊全被拉進來了 |
| `auto cfg = M5.config()` | `auto` = 型別推導（同 TS 的型別推斷）。回傳一包預設硬體設定，想改就在這行之後改 `cfg` 欄位 |
| `M5Cardputer.begin(cfg, true)` | 真正開機：螢幕、電源管理、喇叭。第二個參數 `true` = 順便啟用鍵盤，設 `false` 的話 `Keyboard` 讀不到東西 |
| `setRotation(1)` | **不能省。** 螢幕實體是直的（135×240），轉 1 才變成握著時看到的橫式 240×135 |
| `setTextSize(2)` | 字型**倍率**不是像素。基準 8px，所以 2 = 16px 高 |
| `drawString(s, 10, 56)` | `10, 56` 是**左上角**座標。垂直置中 ≈ (135 − 16) / 2 |
| `Serial.begin(115200)` | 傳輸速率，**必須跟 `make monitor` 一致**，不然是亂碼 |
| `M5Cardputer.update()` | 輪詢硬體狀態存進物件裡。**沒有中斷、沒有事件監聽器**，你要主動去問。漏掉這行鍵盤就是死的 |

## 兩個觀念

**畫上去就留著。** 螢幕自己有記憶，`setup()` 結束後字不會消失。跟瀏覽器每幀重繪的模型完全不同 —— 只在需要改變時才畫。

**`M5` 和 `M5Cardputer` 是兩個物件，不是筆誤。** `M5` 來自底層 M5Unified，`M5Cardputer` 包在外面、多了鍵盤。

## 實測數字

```
Sketch uses 511155 bytes (38%) of program storage space.   上限 1.25MB
Global variables use 25948 bytes (8%) of dynamic memory,
  leaving 301732 bytes for local variables.                 上限 320KB
```

每次編譯瞄一眼第二個數字，突然掉很多就是有東西吃記憶體。

## 踩過的坑

- **序列埠看不到 `booted`** —— 它只在開機那一瞬間印一次。正確順序是**先開監控、再讓機器重開**：`make monitor` 開著，然後電源 OFF → ON。嵌入式沒有捲軸歷史，你不在場就是沒看到。
- **不用手動進下載模式** —— ESP32-S3 的原生 USB 可以自己重置。真的卡住時才需要：電源 OFF → 按住 G0 → 插 USB-C → 放開。
