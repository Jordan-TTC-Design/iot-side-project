#!/usr/bin/env bash
# 在一台全新的 Mac 上重建 Cardputer ADV 開發環境。
# 可重複執行，已裝好的會跳過。
set -euo pipefail

BOARD_URL="https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json"
CORE_VER="m5stack:esp32@3.3.9"          # 需求下限 3.2.2
LIB="M5Cardputer@1.1.1"                 # 會連帶拉 M5Unified / M5GFX / IRremote / LibSSH-ESP32

# --- 1. 找到 arduino-cli ---------------------------------------------------
# 優先用 PATH 上的獨立版；否則用 Arduino IDE 內建的那份。
if command -v arduino-cli >/dev/null 2>&1; then
  CLI="$(command -v arduino-cli)"
elif [ -x "/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli" ]; then
  CLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
else
  echo "找不到 arduino-cli。兩個選擇："
  echo "  a) 安裝 Arduino IDE：https://www.arduino.cc/en/software"
  echo "  b) 只裝命令列版：brew install arduino-cli"
  exit 1
fi
echo "==> arduino-cli: $CLI"
"$CLI" version

# --- 2. 設定檔（板子來源）--------------------------------------------------
CFG="$HOME/.arduinoIDE/arduino-cli.yaml"
mkdir -p "$(dirname "$CFG")"
if [ -f "$CFG" ]; then cp "$CFG" "$CFG.bak.$(date +%s)"; fi
cat > "$CFG" << YAML
board_manager:
    additional_urls:
        - $BOARD_URL
YAML
echo "==> 已寫入 $CFG"

CLI_ARGS=(--config-file "$CFG")

# --- 3. 板子套件 -----------------------------------------------------------
echo "==> 更新板子清單"
"$CLI" "${CLI_ARGS[@]}" core update-index

echo "==> 安裝 $CORE_VER（約 1.5GB，會跑好幾分鐘）"
"$CLI" "${CLI_ARGS[@]}" core install "$CORE_VER"

# --- 4. 函式庫 -------------------------------------------------------------
echo "==> 更新函式庫清單"
"$CLI" "${CLI_ARGS[@]}" lib update-index

echo "==> 安裝 $LIB"
"$CLI" "${CLI_ARGS[@]}" lib install "$LIB"

# --- 5. 驗收 ---------------------------------------------------------------
echo
echo "===== 完成，目前狀態 ====="
"$CLI" "${CLI_ARGS[@]}" core list
"$CLI" "${CLI_ARGS[@]}" lib list
echo
echo "接上 Cardputer 後，用這行找序列埠："
echo "  \"$CLI\" board list"
