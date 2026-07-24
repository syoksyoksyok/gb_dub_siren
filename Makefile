PROJECT := gb_dub_siren
BUILD := build
SRC := src/main.c

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

clean:
	$(RM)

