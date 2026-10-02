#!/usr/bin/env python3
"""產生「今日新聞包」→ sd/goblin/news.tsv

抓 RSS（預設 NPR、BBC）最新幾篇的全文，挑出大考第 3–6 級的字，例句直接用文章原句。
不用 Claude：字義從 6000 字表拿（一字多義時不一定是這篇用的意思）。

檔案格式（Tab 分隔）：
  A  來源  日期  標題                                  ← 一篇文章
  W  索引  級數  單字  KK  中文  原句                    ← 這篇挑出來的字；索引 = vocab.tsv 的行號

用法：python3 tools/build_news.py [每個來源幾篇，預設 3]
"""
import html, os, re, subprocess, sys
from email.utils import parsedate_to_datetime

sys.path.insert(0, os.path.dirname(__file__))
from build_vocab import ROOT, CACHE, fetch, kk

OUT = os.path.join(ROOT, 'sd', 'goblin', 'news.tsv')
VOCAB = os.path.join(ROOT, 'sd', 'goblin', 'vocab.tsv')
FEEDS = [
    ('NPR', 'https://feeds.npr.org/1001/rss.xml'),
    ('BBC', 'https://feeds.bbci.co.uk/news/world/rss.xml'),
]
MAX_WORDS = 10
# 圖說、圖片來源、網站雜訊
JUNK = re.compile(r'hide caption|toggle caption|Getty Images|/AP\b|Image credit|Image source|Copyright|BBC is not responsible|Follow us', re.I)


def curl(url):
    return subprocess.run(['curl', '-fsSL', '-m', '20', '-A', 'Mozilla/5.0', url], capture_output=True, text=True).stdout


def article_text(src, url):
    page = curl(url)
    if src == 'NPR':
        m = re.search(r'id="storytext"(.*?)(<div class="story-footer"|<aside|$)', page, re.S)
        page = m.group(1) if m else page
        page = re.sub(r'<figure.*?</figure>|<div class="caption.*?</div>', ' ', page, flags=re.S)
        paras = re.findall(r'<p>(.*?)</p>', page, re.S)
    else:  # BBC：正文在 data-component="text-block"
        blocks = re.findall(r'data-component="text-block".*?</div>', page, re.S)
        paras = [p for b in blocks for p in re.findall(r'<p[^>]*>(.*?)</p>', b, re.S)]
    text = ' '.join(html.unescape(re.sub(r'<[^>]+>', '', p)).strip() for p in paras)
    return re.sub(r'\s+', ' ', text).strip()


def load_vocab():
    """回傳 {小寫單字: (行號, 級數, 單字, KK, 中文)}"""
    words = {}
    for i, line in enumerate(open(VOCAB, encoding='utf-8')):
        lv, w, k, pos, zh, *_ = line.rstrip('\n').split('\t')
        for h in w.split('/'):
            h = re.sub(r'\(.*?\)', '', h).strip().lower()
            if h and ' ' not in h: words.setdefault(h, (i, int(lv), w, k, zh))
    return words


def lemma(tok, words):
    t = tok.lower()
    if t in words: return t
    m = re.match(r'(.*?)(.)\2(ed|ing)$', t)               # stopped → stop
    if m and m.group(1) + m.group(2) in words: return m.group(1) + m.group(2)
    rules = [('ies', 'y'), ('ied', 'y'), ('s', ''), ('ed', ''), ('ed', 'e'), ('ing', ''), ('ing', 'e'), ('ly', ''), ('ily', 'y')]
    if re.search(r'(s|x|z|ch|sh)es$', t): rules.insert(0, ('es', ''))   # boxes → box；但 refugees 不會變 refuge
    for suf, rep in rules:
        if t.endswith(suf) and t[:-len(suf)] + rep in words: return t[:-len(suf)] + rep


def pick(text, words):
    sents = re.split(r'(?<=[.!?”"])\s+(?=[A-Z“"])', text)
    picked = {}
    for s in sents:
        if JUNK.search(s) or not 40 < len(s) < 220: continue
        for tok in re.findall(r"[A-Za-z]+", s):
            l = lemma(tok, words)
            if l and words[l][1] >= 3 and l not in picked: picked[l] = s
    # 難的字排前面，同級照文章順序
    order = sorted(picked, key=lambda l: -words[l][1])
    return [(l, picked[l]) for l in order[:MAX_WORDS]]


def main():
    per_feed = int(sys.argv[1]) if len(sys.argv) > 1 else 3
    if not os.path.exists(VOCAB): sys.exit('先跑 tools/build_vocab.py')
    words = load_vocab()
    clean = lambda s: re.sub(r'[\t\r\n]+', ' ', s).strip()
    out = []
    for src, feed in FEEDS:
        xml = curl(feed)
        items = []
        for it in re.findall(r'<item>(.*?)</item>', xml, re.S):
            g = lambda t: re.search(f'<{t}>(.*?)</{t}>', it, re.S)
            title, link, date = g('title'), g('link'), g('pubDate')
            if not (title and link): continue
            t = html.unescape(re.sub(r'^<!\[CDATA\[|\]\]>$', '', title.group(1).strip()))
            d = parsedate_to_datetime(date.group(1).strip()) if date else None
            items.append((d, t, link.group(1).strip()))
        items.sort(key=lambda x: x[0].timestamp() if x[0] else 0, reverse=True)
        n = 0
        for d, title, link in items:
            if n >= per_feed: break
            text = article_text(src, link)
            if len(text) < 800: continue          # 影音頁、直播頁之類沒有正文
            got = pick(text, words)
            if len(got) < 4: continue
            out.append('\t'.join(['A', src, d.strftime('%Y-%m-%d') if d else '', clean(title)]))
            for l, s in got:
                i, lv, w, k, zh = words[l]
                out.append('\t'.join(['W', str(i), str(lv), w, k, zh, clean(s)]))
            print(f'{src} {title[:60]}… {len(got)} 字')
            n += 1
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    open(OUT, 'w', encoding='utf-8').write('\n'.join(out) + '\n')
    print(f'→ {os.path.relpath(OUT, ROOT)}  {os.path.getsize(OUT) // 1024} KB')


if __name__ == '__main__':
    main()
