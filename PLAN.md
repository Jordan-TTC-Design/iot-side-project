# 哥布林營地 Cardputer 介面計畫

Cardputer ADV 的主介面：開機看到哥布林營地，左邊營地、右邊一條一條的 app。
取代外觀不好看的 M5Launcher，角色素材來自 `/Users/jordan/code/mac-ant-game`（哥布林營地）。

> 原本的課程（README 的第 04～08 課）先暫停，需要的技術（狀態機、連網、存檔、IR、IMU）直接在這裡邊做邊學。

---

## 決定好的事

| 項目 | 決定 | 原因 |
|---|---|---|
| 架構 | **單一韌體，app 內建成模組** | 按 Esc 就能回主選單、風格統一、共用 Wi-Fi／時間／設定；之後要裝外部 `.bin` 再加「從 SD 安裝」（OTA API） |
| 角色素材 | 營地的 16×16 點陣圖，放大 2 倍 | 不用重畫；用腳本把 PNG 轉 C 陣列 |
| 先不做 | 番茄鐘、便利貼 | 2026-10-02 討論決定 |
| 英文分級 | 大考 6 級，**從第 3 級逐步往上**；可以跳級，但要先通過測驗 | |
| 紅外線：冷氣 | **日立、大金、國際牌、三菱都要**，在設定裡選品牌 | ADV 只能發射、不能學習原本的遙控器，靠 IRremoteESP8266 內建協定 |
| 紅外線：電視 | **LG** | |

---

## 介面（240×135）

可以操作的原型：[`docs/launcher-prototype.html`](docs/launcher-prototype.html)（線上版：https://claude.ai/artifact/Y6zYnGmCfSYs68sL5EuAiY）

```
┌──────────────────────────────────────────────┐
│ 14:32   ◔ 5h 42% ↻1:18   ◑ 週 61%    ▮▮▮▯ │ ← 狀態列
├────────────────┬─────────────────────────────┤
│  ·   ✦    ·    │ ▶ 英語單字        Lv3 · 12 │
│      🔥 ⛺     │   今日新聞          VOA · 1 │
│  [哥布林] [公主]│   守城打字           2,340 │
│  ▓▓▓▓ 草地 ▓▓▓▓│   Claude 額度          42% │
│                │   遙控器             冷 26° │
└────────────────┴─────────────────────────────┘
  左 ~104px 營地動畫         右 ~136px 選單，每條 22px
```

- 營地會反映狀態：額度快用完時哥布林累倒，有新單字時舉牌子。
- 按鍵：`;` 上、`.` 下、`,` 左、`/` 右、Enter 確定、`` ` `` 返回。

---

## App 清單

### 1. 主選單＋營地 ⬜
- 營地動畫：營火、帳篷、2～3 隻哥布林閒晃、公主
- 狀態列：時間、Wi-Fi、Claude 額度、電量
- 選單可以捲動，選中的那條會亮起來

### 2. 英語單字 ⬜
- **字表**：[fullmodel-star/english6000](https://github.com/fullmodel-star/english6000)，是大考中心《高中英文參考詞彙表》，6005 詞，分 6 級，每個字都有繁中字義、例句、例句翻譯
  - 授權寫的是「供教學與學習使用」，自己用沒問題，**不要公開散布資料檔**
- **KK 音標**：用 CMU 美式發音字典（13.5 萬字）轉換，腳本在試驗階段
  - 6005 詞中只有 7 個查不到（okay、stomachache、dragonfly、toothache、antonym、bra、examinee），可以手動補
  - 抽樣 30 字，大約 85～90% 跟一般字典一樣；不一樣的大多是 CMU 收的發音版本不同（例如 restaurant）
  - 待做：-ble／-tle 改成 `bl̩` 這類 KK 慣用寫法；跟 Wiktionary 交叉比對
  - 待做：緊接在主重音前的次重音要拿掉（imply 轉成 `ˏɪmˋplaɪ`，KK 寫 `ɪmˋplaɪ`）
- **正在播放（Spotify）**：橋接程式用 AppleScript 讀寫 Mac 上的 Spotify App（不用 API 金鑰、不用 Premium）。`/np.txt` 歌名歌手進度、`/np/art` 封面縮成 32×32、16 色；機器上像素唱片機，封面當唱片標籤，‹ › 上下首、Enter 暫停、; . 音量。只抓得到這台 Mac 上播的
- **橋接程式常駐**：`./tools/install_bridge.sh`（launchd `cc.goblin.bridge`，log 在 `~/Library/Logs/goblin-bridge.log`）；改了 bridge.py 再跑一次就會重新載入
- **Claude 額度已接上**：2026-10-02 改了 `~/.claude/settings.json` 的 statusLine（前面加 goblin_statusline.py，ccstatusline 照舊）。另一個帳號如果有自己的 `CLAUDE_CONFIG_DIR`，那邊的 settings.json 也要改同一行
- **營火白噪音**：機器上即時合成（布朗噪音＋隨機劈啪），不用音檔；營火時鐘按 f 開關、[ ] 大小聲，劈啪時營火噴火星；關螢幕也繼續播
- **真實天氣**：Open-Meteo（免費、不用金鑰，機器直接抓），每 15 分鐘。營地跟著變：照日出日落分白天黃昏晚上、雲、雨（依雨量）、雪、霧、雷雨閃光；<15°C 發抖、>30°C 冒汗。城市在設定裡選。除錯：序列埠 `WX <代碼> <溫度> <白天>`、`WX off`
- **哥布林合唱團**：7 個角色（平民、敏捷、聰明、壯碩、金皮、混血、公主）。Tab 切三種模式：唱名（機器上加法合成，每個角色的共振峰不同＝不同母音）、錄音（r＋角色鍵，倒數後錄 1.5 秒、自動放大音量，存 `/goblin/choir/rec/N.raw`）、自訂（`/goblin/choir/custom/N.wav`，用 `tools/choir_put.py 3 檔案` 轉檔上傳）。鍵盤當鋼琴：四排鍵＝四個八度（z 排低音、a 排中音、q 排高音、數字排最高），每排從左邊 Do 開始、往右接著往上，Shift 升半音，不用切八度；錄音模式按 Enter 再按琴鍵錄那個角色；錄音和自訂音檔依音高變速播放；4 音複音
- **發音**：Mac 上用 `say -v Samantha` 預錄單字和例句，放在 SD 卡
- **複習**：Leitner 卡片盒間隔複習，進度存在 SD 卡
- **跳級測驗**：每一級抽題測驗，通過才解鎖下一級
- **字型**：要自己做點陣字型，涵蓋 KK 符號（`ə ɚ ɝ ŋ θ ð ʃ ʒ æ ɑ ɔ ʌ ˋ ˏ`）和繁中（或使用 M5GFX 的 efontTW）

### 3. 今日新聞（RSS 學英文）⬜
- **RSS 來源可以設定、可以有多個**。新聞只在自己的裝置上讀，不轉發出去
- 2026-10-02 實測各家 RSS：

  | 來源 | 狀態 | 備註 |
  |---|---|---|
  | NPR `https://feeds.npr.org/1001/rss.xml` | ✅ 今天有更新 | RSS 只有摘要，全文要抓文章頁（`#storytext`） |
  | BBC `https://feeds.bbci.co.uk/news/world/rss.xml` | ✅ 今天有更新 | |
  | CNN `http://rss.cnn.com/rss/edition.rss` | ❌ 停在 2023 年 | |
  | VOA Learning English | ⚠️ 停在 2025-03 | 舊文章庫還在（公共領域、有分級、有慢速 MP3），可以當成「經典文章庫」 |

- **整理不一定要用 Claude**（2026-10-02 用一篇 NPR 文章實測）：
  - 流程：斷詞 → 還原字形 → 對照 6000 字表的級數 → 挑出目前級數以上的字 → 字義從字表拿 → 例句用文章原句
  - 結果：一篇約 1150 字的文章挑出 **39 個第 4～6 級的字**（frantic、vulnerable、chronic、enroll…），都有繁中字義和原句
  - 要修的地方：圖說／圖片來源的雜訊要濾掉；字形還原會誤判（refugees → refuge、literally → literal）
  - **Claude 是加分項**：依上下文挑出正確的字義（例如 coverage 在那篇是「保險範圍」）、翻譯原句、處理字表以外的字
- 流程：Mac 每天產生「今日新聞包」→ 放到 SD 卡，或讓 Cardputer 用 Wi-Fi 下載

### 4. 守城打字 ⬜
- 怪物帶著英文單字朝營地走過來，打對拼字就打倒牠
- 單字拿目前正在背的那批，跟第 2 項共用進度

### 5. Claude 額度 ⬜
- 5 小時用量、每週用量、重置倒數；**可以有多個帳號**，用左右鍵切換
- 資料流（先做 A）：
  - **A**：Mac 上常駐一支小程式（放在這個 repo 的 `bridge/`，用 launchd），開一個區網 HTTP 給 Cardputer 讀 → **不用改哥布林營地**
  - B：之後把資料上傳到 world.blockstudio.cc，出門也看得到 → 要在營地的 `server/` 加一個 endpoint
- 多帳號：每個帳號用不同的 `CLAUDE_CONFIG_DIR` 登入，token 存在 macOS 鑰匙圈，**token 不離開 Mac**
- ⚠️ 用量 API 是非公開的，鑰匙圈項目名稱和回傳格式都要實際在 Mac 上確認

### 6. 遙控器（紅外線）⬜
- 冷氣：開關、溫度、模式（冷氣／除濕／送風／暖氣／自動）、風速、擺風
  - 品牌：日立、大金、國際牌、三菱，在設定裡選
  - 冷氣遙控每次都送出完整狀態，所以機器要記住目前的設定
- LG 電視：電源、音量、頻道、靜音、輸入源、Home、方向鍵

### 7. 營火時鐘 ⬜
- 閒置時切成全螢幕：營火、大時鐘、日期、天氣，哥布林在睡覺
- 可以在設定裡開或關

### 8. 設定 ⬜
- Wi-Fi：搜尋、連線，可以記住多組
- 音量、螢幕亮度
- 自動關螢幕：30 秒／1 分／5 分／**永不**
- 閒置時顯示營火時鐘
- 時區、NTP 對時
- 英文程度（目前級數、跳級測驗）
- Claude 帳號
- 遙控器（冷氣品牌）
- 關於（版本、記憶體、SD 用量）

---

## 之後可以考慮
- **怎麼認得「我的電腦」、換電腦**（2026-10-02 提出，還在想）
  - 現況：Cardputer 用 Bonjour 找 `_goblin._tcp`，**連第一個回應的**，沒有驗證。同網路上別人也跑橋接程式、或自己有兩台都裝，會分不出來；同網路的任何裝置也能讀額度、切歌
  - 換電腦：每台電腦都要裝橋接程式（clone repo、`install_bridge.sh`、改狀態列），Cardputer 顯示的是「目前連到那台」的 Claude 帳號和 Spotify
  - 方案 A：每台電腦一組配對碼，Cardputer 記住多台、只連配對過的，狀態列顯示目前連哪台
  - 方案 B：設定裡列出網路上找到的所有橋接程式，像選 Wi-Fi 一樣挑一台（最簡單，但沒有安全性）
  - 方案 C：每台 Mac 把資料推到雲端（哥布林營地的 server），Cardputer 綁「帳號」而不是綁電腦，只配對一次；在哪台電腦用都會出現，出門在外也看得到。Spotify 控制透過 server 轉給正在播的那台 Mac
- 對講機模式問 Claude：麥克風錄音 → Mac 用 Whisper 轉文字 → Claude → 答案顯示在螢幕上
- 發音練習：念一個單字，用 Whisper 辨識，判斷念得對不對
- Claude Code 的允許／拒絕按鈕：接哥布林營地 server 的 `/claude/asks`、`/answer`
- 營地電子雞：背單字的成果讓營地長大
- BLE 巨集鍵盤、搖一搖（IMU）
- 從 SD 安裝外部 `.bin`（OTA）

---

## 韌體（`GoblinCamp/`）

```bash
python3 tools/make_assets.py          # 角色圖＋字型 → GoblinCamp/assets_gen.h
python3 tools/build_vocab.py          # 6000 字＋KK → sd/goblin/vocab.tsv（不進版控）
make flash SKETCH=GoblinCamp PORT=/dev/cu.usbmodem1101
python3 tools/sd_put.py sd/goblin/vocab.tsv   # 透過 USB 寫進 SD 卡，不用拔卡
```

- 用 8MB flash 的分割表（`PartitionScheme=default_8MB`，app 3.2MB）；韌體 1.67MB
- **燒錄會蓋掉 M5Launcher**。2026-10-02 已備份整份 flash 到 `backup/cardputer-launcher-2026-10-02.bin`（不進版控），還原：
  `esptool --chip esp32s3 -p /dev/cu.usbmodem1101 write-flash 0 backup/cardputer-launcher-2026-10-02.bin`
- 字型：英文和 KK 用 Noto Sans 轉的 VLW 反鋸齒字型；中文用 M5GFX 內建的 efontTW_12（KK 符號它沒有，所以要分開）。6000 字的中文只有「〇」缺字
- v0.1 有：主選單＋營地動畫、英語單字（Leitner、進度存在 SD 卡）、守城打字、營火時鐘、設定（音量、亮度、自動關螢幕、Wi-Fi 掃描連線、NTP 對時）
- 遙控器：IRremoteESP8266 2.9.0（`IRac`）。冷氣每個品牌有好幾種協定，在「型號」列一個一個試；LG 電視用 NEC 碼，直接按鍵模式（方向鍵＝電視方向鍵，p 電源、m 靜音、i 輸入、h Home、= - 音量、] [ 頻道、b 返回）
- 營火時鐘按 Space 切換常亮（不自動關螢幕）
- 守城打字的字牌分 4 條軌道，不會互相蓋住
- **Mac 橋接**（`tools/bridge.py`，port 8787，Bonjour `_goblin._tcp`）：`/usage.txt` 額度、`/news.tsv` 新聞包（超過 6 小時自動重抓）
- **Claude 額度不碰 token**：Claude Code 的狀態列本來就會收到 `rate_limits`（five_hour／seven_day 的 used_percentage、resets_at）。`tools/goblin_statusline.py` 夾在中間存一份到 `~/.goblin/usage/<帳號>.json`，再交給原本的 ccstatusline。只在用 Claude Code 時更新
- 新聞：`tools/build_news.py` 不用 Claude 挑字；Cardputer 上 Tab 從橋接程式更新，Enter 把字加進單字卡，Space 念句子
- **發音**：單字卡 Enter 念單字、Tab 念句子。Mac 的 `say`（Samantha）產生 8-bit 11025Hz PCM；機器先找 SD 卡 `/goblin/audio/{w,s,n}/`，沒有就向橋接程式 `/tts` 要，邊下載邊存進 SD 卡，從 SD 卡串流播放（沒有 PSRAM，3 塊 4KB 輪流）。想離線用，`tools/build_audio.py 3` 一次產生整級，用讀卡機複製
- 待做：背單字的間隔要用日期（現在只照這一輪排序）；跳級測驗；vocab.tsv 換版時舊進度會錯位（之後改成用單字當 key）

## 進度紀錄
- **2026-10-02**：討論方向；找到 6000 字表；驗證 CMU → KK 轉換可行；實測 RSS 來源（VOA、CNN 已停更，改用 NPR、BBC）；不用 Claude 從新聞挑字可行；開始做 HTML 介面原型；HTML 原型 v1 完成（8 個畫面都可以操作）
- **2026-10-02（下午）**：韌體 v0.1 編譯通過；備份整份 flash；SD 傳檔工具
- **2026-10-02（晚上）**：遙控器、營火時鐘常亮、守城字牌分軌、Mac 橋接、Claude 額度、新聞 app（編譯通過，等實機測試）
- **2026-10-02（晚上）**：實機驗證遙控器版面、新聞、Claude 額度（mDNS 找到橋接程式）、發音（下載、存 SD、串流播放）
- **2026-10-02（晚上）**：Spotify 正在播放、橋接程式 launchd 常駐、Claude 額度接上真的資料
- **2026-10-02（晚上）**：營火白噪音、真實天氣場景（7 種天氣實機截圖驗證）
- **2026-10-02（晚上）**：哥布林合唱團（三種模式、鋼琴鍵盤，實機測過錄音和上傳）
