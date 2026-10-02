# Cardputer ADV 練習專案

M5Stack Cardputer ADV（ESP32-S3）的自學紀錄。
> **目前主線：[`GoblinCamp/`](GoblinCamp/)** —— 哥布林營地主選單韌體（英語單字、守城打字、營火時鐘、設定…）。
> 計畫與指令見 [`PLAN.md`](PLAN.md)，介面原型 [`docs/launcher-prototype.html`](docs/launcher-prototype.html)。
> 下面的課程暫停中。

完整路線圖：[`docs/cardputer-roadmap.html`](docs/cardputer-roadmap.html)
（線上版：https://claude.ai/code/artifact/8003f878-2f24-44a3-a981-83d522e1f10a）

## 進度

| 課 | 資料夾 | 主題 | 狀態 |
|---|---|---|---|
| 01 | [`HelloCardputer/`](HelloCardputer/) | 環境、螢幕、序列埠 | ✅ 已燒錄 |
| 01+ | [`scripts/`](scripts/) | 命令列重建環境 | ✅ |
| 02 | [`SpriteDemo/`](SpriteDemo/) | Sprite 雙緩衝 | ✅ 已燒錄 |
| 03 | [`TextInput/`](TextInput/) | 鍵盤輸入框 | ✅ 已燒錄 |
| 04 | — | 主迴圈與狀態機 | ⬜ 下一課 |
| 05 | — | 聲音 → 電子琴 | ⬜ |
| 06 | — | 連網 → Claude 額度記錄器 | ⬜ |
| 07 | — | 存檔與時間 → 電子雞 | ⬜ |
| 08 | — | IMU、紅外線、電量 | ⬜ |

每個資料夾裡的 `README.md` 是該課的實驗紀錄（做了什麼、踩到什麼、量到什麼數字）。

## 指令

```bash
make build   SKETCH=SpriteDemo    # 只編譯
make flash   SKETCH=SpriteDemo    # 編譯並燒錄
make monitor                      # 序列埠監控（Ctrl-C 離開）
make ports                        # 列出接上的裝置

make bin     SKETCH=SpriteDemo    # 產生 dist/SpriteDemo.bin
make bin-all                      # 每一課都產一份到 dist/
```

`SKETCH` 省略時預設 `TextInput`。

## 收進 M5Launcher

`make bin-all` 產出的 `dist/*.bin` 是純 app 映像（不含 bootloader），
可直接複製到 microSD 根目錄，再用 [M5Launcher](https://bmorcelli.github.io/Launcher/)
的 **SD** 選單安裝（或用 WUI 網頁介面的 OTA 直接上傳），之後就能在選單裡切換想跑哪一課。

**怎麼切回 Launcher**：按左上角 **reset**，接著按 **上/下（`;` / `.`）**。
不按的話會直接跑上次選的那支 —— 我們自己的 sketch 裡沒有回 Launcher 的程式碼，
所以進去之後只剩這條路。實機分割表：

```
app0     app  test    0x010000  1344K   ← Launcher 本體
helloc   app  ota_0   0x170000   512K   ← 各課裝在 OTA slot
sprite   app  ota_1   0x1f0000   512K
```

Launcher 放在 `test` 分割區，bootloader 只有在開機瞬間偵測到按鍵才會去開它，
otadata 平常指向 OTA slot。這就是為什麼「開機後才按」沒用，一定要按著開機。

同資料夾的 `*.ino.merged.bin`（4 MB）是含 bootloader 的完整映像，
給 esptool 從 offset 0 燒的。Launcher 兩種都吃，但沒必要為了 500K 的程式搬 4 MB。

## 換一台電腦

```bash
./scripts/setup-arduino.sh
```

需要先有 Arduino IDE（內建 arduino-cli）或 `brew install arduino-cli`。
`~/Library/Arduino15` 那 1.5GB 不用搬，腳本會重新下載。

## 已驗證的版本

| 元件 | 版本 | 需求下限 |
|---|---|---|
| m5stack:esp32 | 3.3.9 | 3.2.2 |
| M5Cardputer | 1.1.1 | 1.1.0 |
| M5Unified | 0.2.21 | 0.2.8 |
| M5GFX | 0.2.28 | 0.2.10 |

FQBN：`m5stack:esp32:m5stack_cardputer` · 序列埠：`/dev/cu.usbmodem101`
