# Cardputer ADV 練習專案

M5Stack Cardputer ADV（ESP32-S3）的自學紀錄。
完整路線圖：[`docs/cardputer-roadmap.html`](docs/cardputer-roadmap.html)
（線上版：https://claude.ai/code/artifact/8003f878-2f24-44a3-a981-83d522e1f10a）

## 進度

| 課 | 資料夾 | 主題 | 狀態 |
|---|---|---|---|
| 01 | [`HelloCardputer/`](HelloCardputer/) | 環境、螢幕、序列埠 | ✅ 已燒錄 |
| 01+ | [`scripts/`](scripts/) | 命令列重建環境 | ✅ |
| 02 | [`SpriteDemo/`](SpriteDemo/) | Sprite 雙緩衝 | ✅ 已燒錄 |
| 03 | — | 鍵盤輸入框 | ⬜ 下一課 |
| 04 | — | 主迴圈與狀態機 | ⬜ |
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
```

`SKETCH` 省略時預設 `HelloCardputer`。

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
