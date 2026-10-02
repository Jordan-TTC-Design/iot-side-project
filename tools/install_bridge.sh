#!/bin/zsh
# 把 Mac 橋接程式裝成開機自動啟動（launchd），當掉會自動重開
#   ./tools/install_bridge.sh          安裝或更新（改了 bridge.py 之後再跑一次就會重新載入）
#   ./tools/install_bridge.sh remove   移除
set -e
LABEL=cc.goblin.bridge
PLIST=~/Library/LaunchAgents/$LABEL.plist
LOG=~/Library/Logs/goblin-bridge.log
ROOT=${0:A:h:h}
PY=$(command -v python3)

launchctl bootout gui/$UID/$LABEL 2>/dev/null || true
if [[ $1 == remove ]]; then rm -f $PLIST; echo "已移除"; exit; fi

mkdir -p ~/Library/LaunchAgents
cat > $PLIST <<PL
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>Label</key><string>$LABEL</string>
  <key>ProgramArguments</key><array><string>$PY</string><string>$ROOT/tools/bridge.py</string></array>
  <key>WorkingDirectory</key><string>$ROOT</string>
  <key>RunAtLoad</key><true/>
  <key>KeepAlive</key><true/>
  <key>StandardOutPath</key><string>$LOG</string>
  <key>StandardErrorPath</key><string>$LOG</string>
  <key>EnvironmentVariables</key><dict><key>PYTHONUNBUFFERED</key><string>1</string></dict>
</dict></plist>
PL
launchctl bootstrap gui/$UID $PLIST
echo "已啟動：$LABEL（log：$LOG）"
