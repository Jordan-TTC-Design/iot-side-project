// 把路線圖片段 + 各課 README 組成一份可離線開啟的完整 HTML。
import { readFileSync, writeFileSync, existsSync, mkdirSync } from 'node:fs'
import { dirname, join } from 'node:path'
import { mdToHtml } from './md2html.mjs'

const ROOT = new URL('..', import.meta.url).pathname
const SRC  = process.argv[2]
const DST  = join(ROOT, 'docs/cardputer-roadmap.html')
const URL_ = 'https://claude.ai/code/artifact/8003f878-2f24-44a3-a981-83d522e1f10a'

if (!SRC || !existsSync(SRC)) { console.error('用法: node scripts/build-docs.mjs <路線圖片段.html>'); process.exit(1) }

// 依 README 出現順序組出「實作紀錄」章節
const LESSONS = [
  { dir: 'HelloCardputer', num: '01' },
  { dir: 'SpriteDemo',     num: '02' },
  { dir: 'TextInput',      num: '03' },
]

const notes = LESSONS
  .filter(l => existsSync(join(ROOT, l.dir, 'README.md')))
  .map(l => `<section class="lesson" id="note-${l.num}">
  <div class="lesson-head"><span class="num">${l.num}</span><h3>實作紀錄 · <code class="inl">${l.dir}/</code></h3></div>
  ${mdToHtml(readFileSync(join(ROOT, l.dir, 'README.md'), 'utf8').replace(/^#\s+.*\n/, ''))}
</section>`).join('\n')

const notesBlock = notes ? `
<div class="stage">
  <span class="tag">實作</span>
  <h2>已完成的課 · 現場筆記</h2>
  <span class="est">由各課 README.md 自動產生</span>
</div>
${notes}
` : ''

const src = readFileSync(SRC, 'utf8')
const head = src.slice(0, src.indexOf('</style>') + 8)
let body = src.slice(src.indexOf('</style>') + 8)

// 把筆記插在附錄之前
body = body.replace('<section class="panel" id="map"', notesBlock + '\n<section class="panel" id="map"')
// TOC 加一個入口
if (notes) body = body.replace('  <h4>附錄</h4>',
  `  <h4>實作紀錄</h4>\n  <ol>\n` +
  LESSONS.filter(l => existsSync(join(ROOT, l.dir, 'README.md')))
    .map(l => `    <li><a href="#note-${l.num}"><span class="n">${l.num}</span><span>${l.dir}</span></a></li>`).join('\n') +
  `\n  </ol>\n  <h4>附錄</h4>`)

const out = `<!doctype html>
<html lang="zh-Hant">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="description" content="給網頁開發者的 M5Stack Cardputer ADV 自學路線圖">
${head}
<style>
  html{-webkit-text-size-adjust:100%;}
  img,svg{max-width:100%;height:auto;}
  .backlink{font-family:var(--font-mono);font-size:11.5px;color:var(--ink-3);
    border-bottom:1px solid var(--rule);padding:10px 0;letter-spacing:.03em;}
  .backlink a{color:var(--ink-2);}
  .lesson h4{font-size:15px;font-weight:700;margin:18px 0 0;color:var(--ink);}
  .lesson h5{font-size:14px;font-weight:600;margin:14px 0 0;color:var(--ink-2);}
</style>
</head>
<body>
<div class="wrap"><p class="backlink">本機副本 · 線上版：<a href="${URL_}">claude.ai/code/artifact/8003f878</a></p></div>
${body}
</body>
</html>`

mkdirSync(dirname(DST), { recursive: true })
writeFileSync(DST, out)
console.log(`✓ ${DST}`)

// artifact 用的片段：同樣內容，但不含 doctype/html/head/body（由平台包）
const FRAG = join(ROOT, 'docs/.artifact-fragment.html')
writeFileSync(FRAG, head + `
<style>
  .lesson h4{font-size:15px;font-weight:700;margin:18px 0 0;color:var(--ink);}
  .lesson h5{font-size:14px;font-weight:600;margin:14px 0 0;color:var(--ink-2);}
</style>
` + body)
console.log(`✓ ${FRAG}`)
console.log(`  併入 ${LESSONS.length} 份 README`)
