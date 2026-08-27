// 極簡 Markdown → HTML，只支援本專案 README 用到的語法。
// 刻意不引外部套件：這個腳本要能在任何裝了 node 的機器上直接跑。
const esc = s => s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;')

function inline(s) {
  return esc(s)
    .replace(/`([^`]+)`/g, '<code class="inl">$1</code>')
    .replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>')
    .replace(/\[([^\]]+)\]\(([^)]+)\)/g, '<a href="$2">$1</a>')
}

export function mdToHtml(md) {
  const lines = md.split('\n')
  const out = []
  let i = 0

  while (i < lines.length) {
    const line = lines[i]

    // 程式碼區塊
    if (line.startsWith('```')) {
      const lang = line.slice(3).trim()
      const body = []
      i++
      while (i < lines.length && !lines[i].startsWith('```')) body.push(lines[i++])
      i++
      out.push(`<figure class="code">${lang ? `<figcaption>${esc(lang)}</figcaption>` : ''}`
        + `<pre><code>${esc(body.join('\n'))}</code></pre></figure>`)
      continue
    }

    // 表格
    if (line.startsWith('|') && lines[i+1]?.match(/^\|[\s:|-]+\|$/)) {
      const cells = r => r.split('|').slice(1, -1).map(c => c.trim())
      const head = cells(line)
      i += 2
      const rows = []
      while (i < lines.length && lines[i].startsWith('|')) rows.push(cells(lines[i++]))
      out.push('<div class="tablewrap"><table><thead><tr>'
        + head.map(h => `<th>${inline(h)}</th>`).join('')
        + '</tr></thead><tbody>'
        + rows.map(r => '<tr>' + r.map(c => `<td>${inline(c)}</td>`).join('') + '</tr>').join('')
        + '</tbody></table></div>')
      continue
    }

    // 標題
    const h = line.match(/^(#{1,4})\s+(.*)$/)
    if (h) { out.push(`<h${h[1].length + 2}>${inline(h[2])}</h${h[1].length + 2}>`); i++; continue }

    // 清單
    if (/^[-*]\s+/.test(line)) {
      const items = []
      while (i < lines.length && /^[-*]\s+/.test(lines[i])) {
        items.push(inline(lines[i].replace(/^[-*]\s+/, '')))
        i++
        // 續行
        while (i < lines.length && /^\s{2,}\S/.test(lines[i])) {
          items[items.length-1] += ' ' + inline(lines[i].trim()); i++
        }
      }
      out.push('<ul class="pts">' + items.map(t => `<li>${t}</li>`).join('') + '</ul>')
      continue
    }

    // 空行
    if (!line.trim()) { i++; continue }

    // 段落（吃到空行為止）
    const para = []
    while (i < lines.length && lines[i].trim()
           && !lines[i].startsWith('```') && !lines[i].startsWith('|')
           && !/^#{1,4}\s/.test(lines[i]) && !/^[-*]\s/.test(lines[i])) {
      para.push(lines[i++])
    }
    out.push(`<p>${inline(para.join(' '))}</p>`)
  }
  return out.join('\n')
}
