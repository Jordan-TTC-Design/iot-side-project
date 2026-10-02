#!/usr/bin/env python3
"""夾在 Claude Code 狀態列中間：把額度（rate_limits）存一份給 Cardputer，再把原本的資料交給你原來的狀態列程式。

~/.claude/settings.json 的 statusLine 改成（原本的指令接在後面）：
  "command": "python3 /Users/jordan/code/iot/tools/goblin_statusline.py npx -y ccstatusline@latest"

存到 ~/.goblin/usage/<帳號>.json；帳號名稱 = CLAUDE_CONFIG_DIR 的資料夾名稱，沒設就是 default。
不會讀取或保存任何登入憑證，只有 Claude Code 本來就給狀態列的資料。
"""
import json, os, subprocess, sys, time

raw = sys.stdin.buffer.read()
try:
    data = json.loads(raw)
    rl = data.get('rate_limits')
    if rl:
        cfg = os.environ.get('CLAUDE_CONFIG_DIR', '')
        name = os.path.basename(cfg.rstrip('/')) if cfg else 'default'
        out_dir = os.path.expanduser('~/.goblin/usage')
        os.makedirs(out_dir, exist_ok=True)
        rec = {'name': name, 'updated': int(time.time()), 'rate_limits': rl}
        tmp = os.path.join(out_dir, f'.{name}.tmp')
        with open(tmp, 'w') as f: json.dump(rec, f)
        os.replace(tmp, os.path.join(out_dir, f'{name}.json'))
except Exception:
    pass   # 狀態列不能因為這裡出錯而壞掉

if len(sys.argv) > 1:
    r = subprocess.run(sys.argv[1:], input=raw, stdout=subprocess.PIPE)
    sys.stdout.buffer.write(r.stdout)
