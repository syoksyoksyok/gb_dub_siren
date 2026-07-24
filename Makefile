PROJECT := syok-dub-siren-gb
BUILD := build
SRC := src/main.c
ROM_TITLE := SYOKDUBSIRENGB

GBDK_HOME ?= C:/Dev_tools/gbdk

ifeq ($(OS),Windows_NT)
LCC := $(if $(GBDK_HOME),$(GBDK_HOME)/bin/lcc,lcc)
RM := powershell -NoProfile -Command "if (Test-Path '$(BUILD)') { Remove-Item -Recurse -Force '$(BUILD)' }"
MKDIR := powershell -NoProfile -Command "New-Item -ItemType Directory -Force '$(BUILD)' | Out-Null"
else
LCC := $(if $(GBDK_HOME),$(GBDK_HOME)/bin/lcc,lcc)
RM := rm -rf $(BUILD)
MKDIR := mkdir -p $(BUILD)
endif

.PHONY: all clean

all: $(BUILD)/$(PROJECT).gb

$(BUILD)/$(PROJECT).gb: $(SRC)
	$(MKDIR)
	$(LCC) -o $@ $<
	powershell -NoProfile -ExecutionPolicy Bypass -File patch-rom-header.ps1 -RomPath $@ -Title $(ROM_TITLE)

clean:
	$(RM)

