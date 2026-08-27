# Cardputer ADV 開發指令。用法：make flash SKETCH=HelloCardputer
CLI    := /Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli
CFG    := $(HOME)/.arduinoIDE/arduino-cli.yaml
FQBN   := m5stack:esp32:m5stack_cardputer
PORT   ?= /dev/cu.usbmodem101
SKETCH ?= TextInput
ROADMAP ?= /private/tmp/claude-501/-Users-jordan-code-iot/51873ada-11de-47ac-8d31-60277002428e/scratchpad/cardputer-roadmap.html

.PHONY: build flash monitor ports setup docs

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
