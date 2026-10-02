#!/usr/bin/env python3
"""Mac 橋接程式：在區網提供 Claude 額度和今日新聞給 Cardputer。

  python3 tools/bridge.py            # 預設 port 8787，用 Bonjour 廣播 _goblin._tcp，Cardputer 會自己找到

GET /usage.txt  → 每行一個帳號，Tab 分隔，給機器好解析：
                  now  <現在 epoch>
                  A    <名稱>  <5h %>  <5h 重置 epoch>  <週 %>  <週重置 epoch>  <最後更新 epoch>
GET /news.tsv   → 今日新聞包（超過 6 小時就在背景重新產生）
GET /tts?t=文字&r=語速 → 念出來的聲音：8-bit 無號、11025Hz、單聲道的原始 PCM（Mac 的 say 產生）
GET /np.txt     → Spotify 正在播放：state  歌名  歌手  專輯  位置秒  長度秒  音量  封面id（Mac 上的 Spotify App，AppleScript）
GET /np/art     → 目前專輯封面，32×32 RGB565（高位元組在前），縮成 16 色像素風
GET /np/cmd?c=play|next|prev|volup|voldown
GET /           → 給人看的簡單狀態
"""
import glob, json, os, subprocess, sys, tempfile, threading, time, zlib
from urllib.parse import parse_qs, urlparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
USAGE_DIR = os.path.expanduser('~/.goblin/usage')
NEWS = os.path.join(ROOT, 'sd', 'goblin', 'news.tsv')
PORT = int(os.environ.get('PORT', 8787))
NEWS_MAX_AGE = 6 * 3600
VOICE = os.environ.get('VOICE', 'Samantha')
RATE = 11025


def tts(text, rate_wpm=165):
    """用 say 念出來，轉成 8-bit 11025Hz 單聲道，回傳去掉 WAV 檔頭的 PCM"""
    with tempfile.TemporaryDirectory() as d:
        aiff, wav = os.path.join(d, 'a.aiff'), os.path.join(d, 'a.wav')
        subprocess.run(['say', '-v', VOICE, '-r', str(rate_wpm), '-o', aiff, text], check=True)
        subprocess.run(['afconvert', '-f', 'WAVE', '-d', f'UI8@{RATE}', '-c', '1', aiff, wav], check=True)
        data = open(wav, 'rb').read()
    i = data.find(b'data')
    n = int.from_bytes(data[i + 4:i + 8], 'little')
    return data[i + 8:i + 8 + n]


news_lock = threading.Lock()

# ─── Spotify（Mac 上的 App，用 AppleScript，不需要 API 金鑰） ───
NP_SCRIPT = '''
if application "Spotify" is not running then return "stopped"
tell application "Spotify"
  if player state is stopped then return "stopped"
  set t to current track
  -- 每個欄位先各自轉成文字；直接放進清單會是「參照」，整串轉文字會失敗
  set vState to player state as string
  set vName to (name of t) as string
  set vArtist to (artist of t) as string
  set vAlbum to (album of t) as string
  set vPos to (player position as integer) as string
  set vDur to ((duration of t) div 1000) as string
  set vVol to (sound volume) as string
  set vArt to (artwork url of t) as string
  set AppleScript's text item delimiters to tab
  return {vState, vName, vArtist, vAlbum, vPos, vDur, vVol, vArt} as string
end tell
'''
NP_CMDS = {
    'play': 'tell application "Spotify" to playpause',
    'next': 'tell application "Spotify" to next track',
    'prev': 'tell application "Spotify" to previous track',
    'volup': 'tell application "Spotify" to set sound volume to (sound volume + 10)',
    'voldown': 'tell application "Spotify" to set sound volume to (sound volume - 10)',
}
art_cache = {'url': None, 'raw': b''}


def osa(script):
    r = subprocess.run(['osascript', '-e', script], capture_output=True, text=True, timeout=5)
    if r.returncode: print('AppleScript 錯誤：', r.stderr.strip(), flush=True)
    return r.stdout.strip()


def now_playing():
    out = osa(NP_SCRIPT)
    f = out.split('\t')
    if len(f) < 8: return 'stopped\n', None
    url = f[7]
    f[7] = str(zlib.crc32(url.encode()))               # 封面 id：換歌才重抓
    return '\t'.join(x.replace('\n', ' ') for x in f) + '\n', url


def album_art(url):
    """下載封面，縮成 32×32、16 色，回傳 RGB565（高位元組在前）"""
    if art_cache['url'] == url: return art_cache['raw']
    from PIL import Image
    import io
    jpg = subprocess.run(['curl', '-fsSL', '-m', '10', url], capture_output=True).stdout
    im = Image.open(io.BytesIO(jpg)).convert('RGB').resize((32, 32), Image.BOX)
    im = im.quantize(16, method=Image.Quantize.MEDIANCUT).convert('RGB')
    raw = bytearray()
    for r, g, b in im.getdata():
        v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        raw += v.to_bytes(2, 'big')
    art_cache.update(url=url, raw=bytes(raw))
    return art_cache['raw']


def usage_lines():
    lines = [f'now\t{int(time.time())}']
    for path in sorted(glob.glob(os.path.join(USAGE_DIR, '*.json'))):
        try:
            r = json.load(open(path))
        except Exception:
            continue
        rl = r.get('rate_limits', {})
        h5, wk = rl.get('five_hour', {}), rl.get('seven_day', {})
        lines.append('\t'.join(str(x) for x in (
            'A', r.get('name', '?'),
            round(h5.get('used_percentage', 0)), int(h5.get('resets_at', 0)),
            round(wk.get('used_percentage', 0)), int(wk.get('resets_at', 0)),
            r.get('updated', 0))))
    return '\n'.join(lines) + '\n'


def refresh_news(force=False):
    if not news_lock.acquire(blocking=False): return
    try:
        age = time.time() - os.path.getmtime(NEWS) if os.path.exists(NEWS) else 1e9
        if force or age > NEWS_MAX_AGE:
            print('重新產生新聞包…', flush=True)
            subprocess.run([sys.executable, os.path.join(ROOT, 'tools', 'build_news.py')], cwd=ROOT)
    finally:
        news_lock.release()


class Handler(BaseHTTPRequestHandler):
    def send(self, body, ctype='text/plain; charset=utf-8'):
        b = body.encode() if isinstance(body, str) else body
        self.send_response(200)
        self.send_header('Content-Type', ctype)
        self.send_header('Content-Length', str(len(b)))
        self.end_headers()
        self.wfile.write(b)

    def do_GET(self):
        if self.path == '/usage.txt':
            self.send(usage_lines())
        elif self.path == '/news.tsv':
            threading.Thread(target=refresh_news, daemon=True).start()
            self.send(open(NEWS, 'rb').read() if os.path.exists(NEWS) else b'')
        elif self.path.startswith('/tts'):
            q = parse_qs(urlparse(self.path).query)
            text = q.get('t', [''])[0][:300]
            if not text: return self.send_error(400)
            wpm = int(q.get('r', ['165'])[0])   # 單字念慢一點
            self.send(tts(text, max(90, min(wpm, 250))), 'application/octet-stream')
        elif self.path == '/np.txt':
            self.send(now_playing()[0])
        elif self.path == '/np/art':
            _, url = now_playing()
            self.send(album_art(url) if url else b'', 'application/octet-stream')
        elif self.path.startswith('/np/cmd'):
            c = parse_qs(urlparse(self.path).query).get('c', [''])[0]
            if c not in NP_CMDS: return self.send_error(400)
            osa(NP_CMDS[c])
            self.send(now_playing()[0])
        elif self.path == '/':
            age = int((time.time() - os.path.getmtime(NEWS)) / 60) if os.path.exists(NEWS) else None
            self.send(f'哥布林營地橋接程式\n\n{usage_lines()}\n新聞包：{"沒有" if age is None else f"{age} 分鐘前"}\n')
        else:
            self.send_error(404)

    def log_message(self, fmt, *args):
        print(f'{self.client_address[0]} {fmt % args}', flush=True)


def main():
    # Bonjour 廣播：Cardputer 用 mDNS 找 _goblin._tcp，不用手動輸入 IP
    adv = subprocess.Popen(['dns-sd', '-R', 'GoblinCamp', '_goblin._tcp', 'local', str(PORT)],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    threading.Thread(target=refresh_news, daemon=True).start()
    print(f'橋接程式在 port {PORT}，額度資料：{USAGE_DIR}', flush=True)
    try:
        ThreadingHTTPServer(('0.0.0.0', PORT), Handler).serve_forever()
    finally:
        adv.terminate()


if __name__ == '__main__':
    main()
