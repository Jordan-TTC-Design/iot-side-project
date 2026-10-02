#!/usr/bin/env python3
"""一次產生某一級所有單字和例句的發音 → sd/goblin/audio/（用讀卡機整個複製進 SD 卡最快）

沒有預先產生也沒關係：Cardputer 念的時候會向 Mac 橋接程式要，拿到後自己存進 SD 卡。

  python3 tools/build_audio.py 3        # 第 3 級（約 1100 字，約 60 MB）
檔名：audio/w/<行號>.raw（單字）、audio/s/<行號>.raw（例句）；行號 = vocab.tsv 的第幾行（從 0 開始）
"""
import os, sys
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0, os.path.dirname(__file__))
from bridge import tts, ROOT

VOCAB = os.path.join(ROOT, 'sd', 'goblin', 'vocab.tsv')
OUT = os.path.join(ROOT, 'sd', 'goblin', 'audio')


def job(args):
    path, text, wpm = args
    if os.path.exists(path): return 0
    data = tts(text, wpm)
    open(path, 'wb').write(data)
    return len(data)


def main():
    level = sys.argv[1] if len(sys.argv) > 1 else '3'
    os.makedirs(os.path.join(OUT, 'w'), exist_ok=True)
    os.makedirs(os.path.join(OUT, 's'), exist_ok=True)
    jobs = []
    for i, line in enumerate(open(VOCAB, encoding='utf-8')):
        lv, w, kk, pos, zh, ex, exzh = line.rstrip('\n').split('\t')
        if lv != level: continue
        head = w.split('/')[0]
        jobs.append((os.path.join(OUT, 'w', f'{i}.raw'), head, 140))
        if ex: jobs.append((os.path.join(OUT, 's', f'{i}.raw'), ex, 165))
    total = 0
    with ThreadPoolExecutor(8) as ex:
        for k, n in enumerate(ex.map(job, jobs), 1):
            total += n
            print(f'\r  {k}/{len(jobs)}  {total // 1024 // 1024} MB', end='', flush=True)
    print(f'\n→ {os.path.relpath(OUT, ROOT)}')


if __name__ == '__main__':
    main()
