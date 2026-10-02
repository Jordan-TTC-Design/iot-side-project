#!/usr/bin/env python3
"""產生 SD 卡用的單字檔 → sd/goblin/vocab.tsv

字表：fullmodel-star/english6000（大考中心《高中英文參考詞彙表》6 級，含繁中字義、例句、翻譯）
      授權寫「供教學與學習使用」，所以產出只放 SD 卡、不進版控。
音標：CMU Pronouncing Dictionary（美式）轉 KK。

每行一個字，欄位用 Tab 分隔：
  level  word  kk  pos  zh  ex  exZh

用法：python3 tools/build_vocab.py
"""
import json, os, re, subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CACHE = os.path.join(ROOT, 'tools', '.cache')
OUT = os.path.join(ROOT, 'sd', 'goblin', 'vocab.tsv')
SRC = 'https://raw.githubusercontent.com/fullmodel-star/english6000/main/files/'
CMU = 'https://raw.githubusercontent.com/cmusphinx/cmudict/master/cmudict.dict'

# ---------- ARPAbet → KK ----------
V = {'AA': 'ɑ', 'AE': 'æ', 'AO': 'ɔ', 'AW': 'aʊ', 'AY': 'aɪ', 'EH': 'ɛ', 'EY': 'e', 'IH': 'ɪ', 'IY': 'i',
     'OW': 'o', 'OY': 'ɔɪ', 'UH': 'ʊ', 'UW': 'u'}
C = {'B': 'b', 'CH': 'tʃ', 'D': 'd', 'DH': 'ð', 'F': 'f', 'G': 'g', 'HH': 'h', 'JH': 'dʒ', 'K': 'k', 'L': 'l',
     'M': 'm', 'N': 'n', 'NG': 'ŋ', 'P': 'p', 'R': 'r', 'S': 's', 'SH': 'ʃ', 'T': 't', 'TH': 'θ', 'V': 'v',
     'W': 'w', 'Y': 'j', 'Z': 'z', 'ZH': 'ʒ'}
# 英文合法的字首子音群：重音記號要放在整個子音群前面（ex-plain → ɪkˋsplen）
ONSETS = {tuple(o.split()) for o in (
    'P L|P R|T R|K L|K R|K W|B L|B R|D R|G L|G R|F L|F R|TH R|SH R|S P|S T|S K|S M|S N|S L|S W|'
    'S P L|S P R|S T R|S K R|S K W|P Y|B Y|K Y|M Y|F Y|V Y|HH Y|D W|T W|G W|TH W').split('|')}


def vowel(p, s, last):
    if p == 'AH': return 'ʌ' if s in '12' else 'ə'
    if p == 'ER': return 'ɝ' if s in '12' else 'ɚ'
    if p == 'IY' and s == '0' and last: return 'ɪ'      # happy → ˋhæpɪ（KK 慣例）
    return V[p]


def kk(arpa):
    ph = arpa.split()
    stresses = [p[-1] for p in ph if p[-1].isdigit()]
    out, marks = [], {}
    nth = -1
    for i, p in enumerate(ph):
        if not p[-1].isdigit():
            out.append((C[p], True, p)); continue
        nth += 1
        base, s = p[:-1], p[-1]
        # 次重音緊接在主重音前面時拿掉（imply：ɪmˋplaɪ，不是 ˏɪmˋplaɪ）
        if s == '2' and nth + 1 < len(stresses) and stresses[nth + 1] == '1': s = '0'
        if len(stresses) > 1 and s in '12':
            j, cons = len(out), []
            while j > 0 and out[j - 1][1]:
                j -= 1; cons.insert(0, out[j][2])
            k = len(cons)
            while k > 1 and tuple(cons[-k:]) not in ONSETS: k -= 1
            if j == 0: k = len(cons)
            marks[len(out) - k] = 'ˋ' if s == '1' else 'ˏ'
        out.append((vowel(base, s, i == len(ph) - 1), False, base))
    return ''.join(marks.get(i, '') + sym for i, (sym, _, _) in enumerate(out))


def fetch(url, name):
    path = os.path.join(CACHE, name)
    if not os.path.exists(path):
        print('下載', url); subprocess.run(['curl', '-fsSL', '-o', path, url], check=True)
    return path


def main():
    os.makedirs(CACHE, exist_ok=True)
    words = json.load(open(fetch(SRC + 'words_full.json', 'words_full.json')))
    extra = json.load(open(fetch(SRC + 'content_progress.json', 'content_progress.json')))
    cmu = {}
    for line in open(fetch(CMU, 'cmudict.dict')):
        w, pr = line.split(' ', 1)
        cmu.setdefault(w, pr.split('#')[0].strip())

    rows, missing = [], []
    for w in words:
        c = extra.get(w['word'].lower(), {})
        zh = w['zh_tw'] or c.get('zh_tw', '')
        ex = w['ex'] or c.get('ex', '')
        exzh = w['exZh'] or c.get('exZh', '')
        head = re.split(r'[/(]', w['word'])[0].strip().lower()   # "a/an" → a，"am/a.m." → am
        toks = head.split()
        if toks and all(t in cmu for t in toks):
            phon = ' '.join(kk(cmu[t]) for t in toks)
        else:
            phon = ''; missing.append(w['word'])
        clean = lambda s: re.sub(r'[\t\r\n]+', ' ', s).strip()
        rows.append((w['level'], clean(w['word']), phon, clean(w['pos']), clean(zh), clean(ex), clean(exzh)))

    rows.sort(key=lambda r: (r[0], r[1].lower()))
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, 'w', encoding='utf-8') as f:
        for r in rows: f.write('\t'.join(str(x) for x in r) + '\n')
    print(f'→ {os.path.relpath(OUT, ROOT)}  {len(rows)} 字  {os.path.getsize(OUT) // 1024} KB')
    print('沒有 KK 的字：', ', '.join(missing) or '無')

    # 檢查中文字型涵蓋率（efontTW_12 沒有的字在機器上會變空白）
    font_chars = efont_chars()
    if font_chars:
        used = {ch for r in rows for s in (r[4], r[6]) for ch in s if ord(ch) > 0x2E80}
        lack = sorted(ch for ch in used if ord(ch) not in font_chars)
        print(f'中文用到 {len(used)} 個字，efontTW_12 缺 {len(lack)} 個：', ''.join(lack[:80]))


def efont_chars():
    """讀 M5GFX 的 efontTW_12（u8g2 格式），回傳有收的 unicode 集合。找不到就略過檢查。"""
    path = os.path.expanduser('~/Documents/Arduino/libraries/M5GFX/src/lgfx/Fonts/efont/lgfx_efont_tw.c')
    if not os.path.exists(path): return None
    src = open(path).read()
    m = re.search(r'lgfx_efont_tw_12\[\d+\] =\s*(.*?")\s*;\s*\n', src, re.S)
    b = bytearray()
    for lit in re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(1)):
        i = 0
        while i < len(lit):
            if lit[i] == '\\':
                n = lit[i + 1]
                if n in '01234567':
                    j, s = i + 1, ''
                    while j < len(lit) and len(s) < 3 and lit[j] in '01234567': s += lit[j]; j += 1
                    b.append(int(s, 8)); i = j; continue
                b.append({'n': 10, 't': 9, 'r': 13, '"': 34, '\\': 92, "'": 39, '?': 63, 'a': 7}[n]); i += 2; continue
            b.append(ord(lit[i])); i += 1
    p = 23 + (b[21] << 8 | b[22])
    while (b[p + 2] << 8 | b[p + 3]) != 0xFFFF: p += 4
    p += 4
    have = set(range(0x20, 0x100))
    while p < len(b) - 3 and (b[p] << 8 | b[p + 1]):
        have.add(b[p] << 8 | b[p + 1]); p += b[p + 2]
    return have


if __name__ == '__main__':
    main()
