#!/usr/bin/env python3
"""把任何音檔（mp3、m4a、wav、aiff…）轉成合唱團用的格式，傳到某個角色。

  python3 tools/choir_put.py 3 ~/Downloads/meow.m4a      # 第 3 個角色（聰明）
轉成 16kHz 單聲道 16-bit WAV，最長 2 秒，寫到 SD 卡 /goblin/choir/custom/3.wav（透過 USB，機器要在跑 GoblinCamp）
"""
import os, subprocess, sys, tempfile, wave
sys.path.insert(0, os.path.dirname(__file__))

MAX_SEC = 2.0


def main():
    if len(sys.argv) != 3 or sys.argv[1] not in '1234567': sys.exit(__doc__)
    slot, src = sys.argv[1], sys.argv[2]
    with tempfile.TemporaryDirectory() as d:
        raw = os.path.join(d, 'a.wav')
        subprocess.run(['afconvert', '-f', 'WAVE', '-d', 'LEI16@16000', '-c', '1', src, raw], check=True)
        out = os.path.join(d, f'{slot}.wav')
        with wave.open(raw) as r, wave.open(out, 'wb') as w:
            w.setnchannels(1); w.setsampwidth(2); w.setframerate(16000)
            w.writeframes(r.readframes(int(16000 * MAX_SEC)))
        subprocess.run([sys.executable, os.path.join(os.path.dirname(__file__), 'sd_put.py'), out, f'/goblin/choir/custom/{slot}.wav'], check=True)


if __name__ == '__main__':
    main()
