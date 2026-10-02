#!/usr/bin/env python3
"""產生 GoblinCamp 韌體要用的素材 → GoblinCamp/assets_gen.h

- 角色圖：哥布林營地（mac-ant-game）的 16×16 點陣圖，轉成 RGB565，透明色用 0xF81F
- 字型：Noto Sans（OFL）轉成 VLW 反鋸齒字型，含 KK 音標符號。中文另外用 M5GFX 內建的 efontTW_12

用法：python3 tools/make_assets.py
"""
import os, subprocess
from PIL import Image, ImageFont, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CACHE = os.path.join(ROOT, 'tools', '.cache')
OUT = os.path.join(ROOT, 'GoblinCamp', 'assets_gen.h')
GAME = os.environ.get('GOBLIN_GAME', '/Users/jordan/code/mac-ant-game/mac/Resources')
FONT_URL = 'https://github.com/google/fonts/raw/main/ofl/notosans/NotoSans%5Bwdth%2Cwght%5D.ttf'
KEY = 0xF81F

KK = 'əɚɝŋθðʃʒæɑɔʌɪʊɛˋˏ'
EXTRA = '·°‹›…–—’‘“”↻'
ASCII = ''.join(chr(c) for c in range(0x20, 0x7F))


def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def sprite(name, path, box=None):
    im = Image.open(path).convert('RGBA')
    if box: im = im.crop(box)
    w, h = im.size
    px = []
    for y in range(h):
        for x in range(w):
            r, g, b, a = im.getpixel((x, y))
            if a < 128: px.append(KEY); continue
            c = rgb565(r, g, b)
            px.append(c ^ 0x0020 if c == KEY else c)
    body = ','.join('0x%04X' % p for p in px)
    return f'// {os.path.relpath(path, GAME)} {w}x{h}\nstatic const uint16_t SPR_{name}[{w*h}] PROGMEM = {{{body}}};\nstatic const int SPR_{name}_W = {w}, SPR_{name}_H = {h};\n'


def vlw(name, size, weight, chars):
    """VLW 格式：24 bytes 檔頭 + 每字 28 bytes 度量 + 每字 8-bit alpha 點陣。字要依 unicode 排序。"""
    font = ImageFont.truetype(os.path.join(CACHE, 'NotoSans.ttf'), size)
    font.set_variation_by_axes([weight, 100])
    chars = sorted(set(chars))
    metrics, bitmaps = [], []
    for ch in chars:
        adv = round(font.getlength(ch))
        x0, y0, x1, y1 = font.getbbox(ch, anchor='ls')
        w, h = max(0, x1 - x0), max(0, y1 - y0)
        if ch == ' ' or w == 0 or h == 0:
            metrics.append((ord(ch), 0, 0, adv, 0, 0)); bitmaps.append(b''); continue
        im = Image.new('L', (w, h), 0)
        ImageDraw.Draw(im).text((-x0, -y0), ch, font=font, fill=255, anchor='ls')
        metrics.append((ord(ch), h, w, adv, -y0, x0)); bitmaps.append(im.tobytes())
    asc, desc = -font.getbbox('d', anchor='ls')[1], font.getbbox('p', anchor='ls')[3]
    be = lambda v: (v & 0xFFFFFFFF).to_bytes(4, 'big')
    data = b''.join(be(v) for v in (len(chars), 11, size, 0, asc, desc))
    for u, h, w, adv, top, left in metrics:
        data += b''.join(be(v) for v in (u, h, w, adv, top, left, 0))
    data += b''.join(bitmaps)
    body = ','.join(str(b) for b in data)
    return f'// Noto Sans {size}px wght {weight}，{len(chars)} 字\nstatic const uint8_t VLW_{name}[{len(data)}] PROGMEM = {{{body}}};\n'


def main():
    os.makedirs(CACHE, exist_ok=True)
    ttf = os.path.join(CACHE, 'NotoSans.ttf')
    if not os.path.exists(ttf): subprocess.run(['curl', '-fsSL', '-o', ttf, FONT_URL], check=True)
    C = lambda p: os.path.join(GAME, 'Characters', p)
    parts = ['// 由 tools/make_assets.py 產生，不要手改\n#pragma once\n#include <Arduino.h>\n',
             f'static const uint16_t SPR_KEY = 0x{KEY:04X};\n',
             sprite('WORKER', C('goblin/worker.png')),
             sprite('SCOUT', C('goblin/scout.png')),
             sprite('SAGE', C('goblin/sage.png')),
             sprite('QUEEN', C('goblin/queen.png'), (0, 0, 64, 16)),          # 只要第一排（正面）
             sprite('ZOMBIE', C('undead/worker.png')),                          # 守城遊戲的敵人
             sprite('OGRE', C('undead/brute.png')),
             sprite('TENT', os.path.join(GAME, 'Camps/tent/stage2.png'), (0, 0, 32, 24)),  # 營火＋主帳篷
             vlw('SMALL', 10, 500, ASCII + EXTRA),
             vlw('BODY', 12, 400, ASCII + KK + EXTRA),
             vlw('BODYB', 12, 700, ASCII + EXTRA),
             vlw('BIG', 22, 700, ASCII),
             vlw('CLOCK', 40, 700, '0123456789:-'),
             ]
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    open(OUT, 'w').write('\n'.join(parts))
    print(f'→ {os.path.relpath(OUT, ROOT)}  {os.path.getsize(OUT) // 1024} KB 原始碼')


if __name__ == '__main__':
    main()
