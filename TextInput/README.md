# 03 · 鍵盤輸入框

打字、退格（Del）、清空（Tab）、送出（Enter）。上方累積送出過的內容，游標每 500ms 閃一次。

## 燒錄

```bash
make flash SKETCH=TextInput
```

## 三個 API，記住就夠

```cpp
M5Cardputer.Keyboard.isChange()    // 這一輪跟上一輪不同 → 等同 watch()
M5Cardputer.Keyboard.isPressed()   // 現在有沒有鍵被按著
M5Cardputer.Keyboard.keysState()   // 拿到「按了什麼」
```

`keysState()` 回傳 `Keyboard_Class::KeysState`，完整定義在
`~/Documents/Arduino/libraries/M5Cardputer/src/utility/Keyboard/Keyboard.h:77`：

| 欄位 | 型別 | 內容 |
|---|---|---|
| `word` | `vector<char>` | 可見字元，**包含空白**（`Keyboard.cpp:186` 註解寫明 including space） |
| `hid_keys` | `vector<uint8_t>` | 原始 HID 鍵碼 |
| `del` `enter` `tab` `space` | `bool` | 這些鍵是不是被按了 |
| `fn` `ctrl` `shift` `opt` `alt` | `bool` | 修飾鍵狀態 |
| `modifiers` | `uint8_t` | 修飾鍵位元遮罩 |

**為什麼 `word` 是陣列不是單一字元**：鍵盤矩陣可以同時偵測多鍵，一次 update 可能拿到好幾個字。

## 事件 vs 狀態

`isChange()` 是效能與正確性的關鍵。沒有它，按著一個鍵不放會每圈迴圈重複加入同一個字 ——
它讓「按下」只觸發一次，等於防彈跳（debounce）。

**但遊戲不能用它。** 要連續移動改用狀態查詢：

```cpp
if (M5Cardputer.Keyboard.isKeyPressed('a')) player.x -= 2;   // 按著就一直動
```

輸入框要事件，遊戲要狀態。

## 踩過的坑：`st` 的作用域

原本想在 `loop()` 裡加清空功能，寫成這樣 —— **編譯不過**：

```cpp
bool poll() {
  auto st = M5Cardputer.Keyboard.keysState();   // st 只活在這對 {} 裡
  ...
}                                                // ← 到這裡就消失了

void loop() {
  if (st.tab) input.value = "";   // ✗ 編譯器不認識 st
}
```

C++ 沒有 JS 的 hoisting 或閉包，變數活在宣告它的那對大括號內，離開就沒了。

正解是放進 `poll()` —— 而且**本來就該放那裡**：清空是輸入框自己的行為，不是外面的事。
「行為跟資料放在一起」這個原則，之後寫電子琴、電子雞都適用。

## 踩過的坑：燒不進去

改完之後燒錄失敗：

```
A fatal error occurred: Failed to connect to ESP32-S3: No serial data received.
```

診斷過程與結論：

| 檢查 | 指令 | 這次的結果 |
|---|---|---|
| 有沒有程序搶埠 | `lsof /dev/cu.usbmodem*` | 有殘留 esptool，`pkill -f esptool` 清掉 |
| 裝置在不在 | `ls /dev/cu.usbmodem*` | 在 |
| USB 描述符 | `system_profiler SPUSBDataType` | Espressif `303a:1001` USB JTAG/serial debug unit |
| 換連線策略 | `esptool --before no-reset\|usb-reset read-mac` | 三種全失敗 → 晶片不在 bootloader |

**真正的解法是手動進下載模式**，而且順序要嚴格：

1. 拔掉 USB-C，等 3 秒
2. 電源開關撥 **OFF**
3. **先按住 G0 不放**
4. 保持按著，插上 USB-C
5. 等 2 秒才放開 G0

**兩個教訓：**

- **不要兩個人同時跑 `make flash`** —— 兩個 esptool 搶同一個埠必定失敗。
- **進了下載模式就直接燒，不要先探測。** `esptool read-mac` 結束時預設會 `Hard resetting`，
  等於把機器踢出下載模式，白費剛做的手動步驟。真要診斷就加 `--after no-reset`。

平常不用這樣是因為 ESP32-S3 的原生 USB 能靠 DTR/RTS 自動重置，但這要靠當時執行中的韌體
正常在跑 USB CDC。裝置狀態一亂就會失效，手動下載模式是繞過韌體、直接從硬體層進 bootloader 的萬用解。

## 可以動手改

| 改什麼 | 怎麼做 |
|---|---|
| 密碼模式 | render 時把 `input.value` 換成等長的 `*` |
| 上一則歷史 | 存 `lastSubmitted`，按 `fn` 時填回 `input.value` |
| 多行編輯 | `history` 改成可編輯，用 `fn` + 上下鍵選行 |

## 實測數字

```
Sketch uses 518667 bytes (39%)
Global variables use 26420 bytes (8%), leaving 301260 bytes
```

## 之後會用到

`TextInput` 這個 struct 可以整段複製走 —— 它只做三件事：吃按鍵、維護 `value`、Enter 時回傳 true。
第 06 課填 WiFi 密碼、第 07 課電子雞取名字都會用到。
