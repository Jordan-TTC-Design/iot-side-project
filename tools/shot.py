#!/usr/bin/env python3
"""從 Cardputer 抓畫面截圖、模擬按鍵（除錯用，機器要在跑 GoblinCamp）。

  python3 tools/shot.py out.png                 # 截圖
  python3 tools/shot.py out.png down down ok    # 先按鍵再截圖（每鍵間隔 0.3 秒）
按鍵名稱：up down left right ok back space del tab，或單一字元（例如 3、a）
"""
import os, sys, time, termios
from PIL import Image
sys.path.insert(0, os.path.dirname(__file__))
from sd_put import open_port, readline, expect


def main():
    out, keys = sys.argv[1], sys.argv[2:]
    _, fd = open_port()
    time.sleep(0.3); termios.tcflush(fd, termios.TCIFLUSH)
    for k in keys:
        os.write(fd, f'KEY {k}\n'.encode()); expect(fd, 'OK'); time.sleep(0.3)
    time.sleep(0.2)
    os.write(fd, b'SHOT\n')
    n = int(expect(fd, 'SHOT').split()[1])
    buf = b''
    while len(buf) < n: buf += os.read(fd, n - len(buf))
    im = Image.new('RGB', (240, 135))
    px = im.load()
    for i in range(240 * 135):
        v = buf[2 * i] << 8 | buf[2 * i + 1]          # 畫布裡是高位元組在前
        px[i % 240, i // 240] = ((v >> 11) << 3, ((v >> 5) & 63) << 2, (v & 31) << 3)
    im.resize((720, 405), Image.NEAREST).save(out)
    print('→', out)


if __name__ == '__main__':
    main()
