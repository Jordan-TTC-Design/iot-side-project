#!/usr/bin/env python3
"""透過 USB 把檔案寫進 Cardputer 的 SD 卡（機器要在跑 GoblinCamp 韌體）。

用法：
  python3 tools/sd_put.py sd/goblin/vocab.tsv            # 寫到 /goblin/vocab.tsv
  python3 tools/sd_put.py 本機檔案 /SD上的路徑
  python3 tools/sd_put.py --ls /goblin                    # 列出 SD 卡上的檔案
環境變數 PORT 可以指定序列埠（預設自動找 /dev/cu.usbmodem*）
"""
import glob, os, select, sys, termios, time


def open_port():
    port = os.environ.get('PORT') or (sorted(glob.glob('/dev/cu.usbmodem*')) or [None])[0]
    if not port: sys.exit('找不到 Cardputer：USB 有接上嗎？')
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY)
    attr = termios.tcgetattr(fd)
    attr[0] = 0                                        # iflag：原始輸入
    attr[1] = 0                                        # oflag
    attr[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
    attr[3] = 0                                        # lflag：不回顯、不分行
    attr[4] = attr[5] = termios.B115200
    termios.tcsetattr(fd, termios.TCSANOW, attr)
    return port, fd


def readline(fd, timeout):
    buf, end = b'', time.time() + timeout
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.1)
        if not r: continue
        c = os.read(fd, 1)
        if c == b'\n': return buf.decode('utf-8', 'replace').strip()
        buf += c
    return None


def expect(fd, prefix, timeout=5):
    """讀到以 prefix 開頭的那行；開機訊息之類的雜訊略過"""
    end = time.time() + timeout
    while time.time() < end:
        line = readline(fd, end - time.time())
        if line is None: break
        if line.startswith('ERR'): sys.exit('機器回報：' + line)
        if line.startswith(prefix): return line
    sys.exit(f'等不到 {prefix}：機器有在跑 GoblinCamp 嗎？')


def main():
    args = sys.argv[1:]
    if not args: sys.exit(__doc__)
    port, fd = open_port()
    time.sleep(0.3)
    termios.tcflush(fd, termios.TCIFLUSH)
    if args[0] == '--ls':
        os.write(fd, f'LS {args[1] if len(args) > 1 else "/"}\n'.encode())
        while (line := readline(fd, 3)) not in (None, 'END'): print(line)
        return
    src = args[0]
    dst = args[1] if len(args) > 1 else '/' + os.path.relpath(src, 'sd') if src.startswith('sd/') else '/' + os.path.basename(src)
    data = open(src, 'rb').read()
    print(f'{src} → {port}:{dst}（{len(data) // 1024} KB）')
    os.write(fd, f'PUT {dst} {len(data)}\n'.encode())
    expect(fd, 'READY')
    t0, sent = time.time(), 0
    while sent < len(data):
        chunk = data[sent:sent + 512]
        os.write(fd, chunk)
        sent += len(chunk)
        expect(fd, 'OK')
        print(f'\r  {sent * 100 // len(data)}%', end='', flush=True)
    expect(fd, 'DONE')
    print(f'\r  完成，{time.time() - t0:.1f} 秒')


if __name__ == '__main__':
    main()
