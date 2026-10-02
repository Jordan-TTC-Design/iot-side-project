// 透過 USB 序列埠把檔案寫進 SD 卡，不用拔卡。對應 Mac 端的 tools/sd_put.py
//   主機送 "PUT /goblin/vocab.tsv 706543\n" → 機器回 "READY"
//   之後每收 512 bytes 回 "OK <已收>"，收完回 "DONE <大小>"
//   "LS /goblin\n" → 列出檔案
//   "SHOT\n" → 傳回目前畫面（RGB565），"KEY down\n" → 模擬按鍵，\"GO 5\n\" → 跳到某個 app；給 tools/shot.py 除錯用
#pragma once

void serialPut(const String& path, size_t size) {
  if (!sdOk) { Serial.println("ERR 沒有 SD 卡"); return; }
  for (int i = 1; i < (int)path.length(); i++)
    if (path[i] == '/') SD.mkdir(path.substring(0, i));
  String tmp = path + ".part";
  File f = SD.open(tmp, FILE_WRITE);
  if (!f) { Serial.println("ERR 無法建立檔案"); return; }

  R(0, 0, W, H, P::panel);
  text("從 Mac 接收檔案…", 120, 64, P::amber, F_BODY, CENTER);
  text(path, 120, 82, P::dim, F_BODY, CENTER);
  cv.pushSprite(0, 0);

  Serial.println("READY");
  static uint8_t buf[512];
  size_t got = 0;
  uint32_t t = millis();
  while (got < size && millis() - t < 5000) {
    size_t want = min(sizeof(buf), size - got), n = 0;
    while (n < want && millis() - t < 5000) {
      int a = Serial.available();
      if (a > 0) { n += Serial.readBytes(buf + n, min((size_t)a, want - n)); t = millis(); }
      else delay(1);
    }
    f.write(buf, n);
    got += n;
    Serial.printf("OK %u\n", (unsigned)got);
  }
  f.close();
  if (got != size) { SD.remove(tmp); Serial.println("ERR 逾時"); toast("傳檔中斷"); return; }
  SD.remove(path);
  SD.rename(tmp, path);
  Serial.printf("DONE %u\n", (unsigned)got);
  if (path == vocab.path) vocab.load();
  toast("收到 " + path.substring(path.lastIndexOf('/') + 1));
}

void serialTick() {
  static String line;
  while (Serial.available()) {
    char c = Serial.read();
    if (c != '\n') { if (line.length() < 200) line += c; continue; }
    line.trim();
    if (line.startsWith("PUT ")) {
      int sp = line.lastIndexOf(' ');
      serialPut(line.substring(4, sp), line.substring(sp + 1).toInt());
    } else if (line == "SHOT") {
      size_t n = W * H * 2;
      Serial.printf("SHOT %u\n", (unsigned)n);
      Serial.write((const uint8_t*)cv.getBuffer(), n);
      Serial.flush();
    } else if (line.startsWith("KEY ")) {
      String k = line.substring(4);
      static const char* const names[] = {"up", "down", "left", "right", "ok", "back", "space", "del", "tab"};
      KeyEv e{K_CHAR, k.length() ? k[0] : (char)0};
      for (int i = 0; i < 9; i++) if (k == names[i]) e = {(Key)i, 0};
      lastInput = millis();
      cur->key(e);
      Serial.println("OK");
    } else if (line.startsWith("GO ")) {   // 直接跳到某個 app（AppId 的數字），測試用
      int id = line.substring(3).toInt();
      if (id >= 0 && id < A_COUNT) { lastInput = millis(); go((AppId)id); }
      Serial.println("OK");
    } else if (line.startsWith("LS")) {
      String dir = line.length() > 3 ? line.substring(3) : String("/");
      File d = SD.open(dir);
      while (d) { File e = d.openNextFile(); if (!e) break; Serial.printf("%s\t%u\n", e.name(), (unsigned)e.size()); e.close(); }
      Serial.println("END");
    }
    line = "";
  }
}
