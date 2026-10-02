# Cardputer ADV 開發指令。用法：make flash SKETCH=HelloCardputer
CLI    := /Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli
CFG    := $(HOME)/.arduinoIDE/arduino-cli.yaml
FQBN   := m5stack:esp32:m5stack_cardputer
PORT   ?= /dev/cu.usbmodem101
SKETCH ?= TextInput
# GoblinCamp 有字型和角色圖，超過預設 1.25MB，改用 8MB flash 的分割表（app 3.2MB）
ifeq ($(SKETCH),GoblinCamp)
FQBN := $(FQBN):FlashSize=8M,PartitionScheme=default_8MB
endif
BUILD  ?= build
DIST   ?= dist
# 所有課程資料夾（有同名 .ino 的就算一課）
SKETCHES := $(sort $(patsubst %/,%,$(dir $(wildcard */*.ino))))
ROADMAP ?= /private/tmp/claude-501/-Users-jordan-code-iot/51873ada-11de-47ac-8d31-60277002428e/scratchpad/cardputer-roadmap.html

.PHONY: build flash monitor ports setup docs bin bin-all

build:      ## 只編譯，不燒錄
	"$(CLI)" --config-file "$(CFG)" compile --fqbn $(FQBN) $(SKETCH)

flash:      ## 編譯並燒錄
	"$(CLI)" --config-file "$(CFG)" compile --fqbn $(FQBN) -u -p $(PORT) $(SKETCH)

monitor:    ## 開序列埠監控（Ctrl-C 離開）
	"$(CLI)" --config-file "$(CFG)" monitor -p $(PORT) -c baudrate=115200

ports:      ## 列出接上的裝置
	"$(CLI)" --config-file "$(CFG)" board list

setup:      ## 在新電腦上重建開發環境
	./scripts/setup-arduino.sh

docs:       ## 重新產生 docs/（會併入各課 README.md）
	node scripts/build-docs.mjs "$(ROADMAP)"

bin:        ## 產生 M5Launcher 用的 dist/$(SKETCH).bin
	"$(CLI)" --config-file "$(CFG)" compile --fqbn $(FQBN) --output-dir "$(BUILD)/$(SKETCH)" $(SKETCH)
	@mkdir -p "$(DIST)"
	@cp "$(BUILD)/$(SKETCH)/$(SKETCH).ino.bin" "$(DIST)/$(SKETCH).bin"
	@echo "→ $(DIST)/$(SKETCH).bin  ($$(du -h "$(DIST)/$(SKETCH).bin" | cut -f1))"

bin-all:    ## 把每一課都產生一份 .bin 到 dist/
	@for s in $(SKETCHES); do $(MAKE) --no-print-directory bin SKETCH=$$s || exit 1; done
	@echo
	@echo "複製到 SD 卡根目錄，再用 Launcher 的 SD 選單安裝："
	@ls -1 "$(DIST)"
