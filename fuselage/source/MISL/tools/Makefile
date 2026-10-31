# Standalone SDK host tool. SEE: (BIN2H.md, Building)
.DEFAULT_GOAL := all
.DELETE_ON_ERROR:
ifeq ($(origin CC),default)
CC := clang
endif
CC ?= clang
CFLAGS ?= -O2
BUILD_DIR ?= ../../../build/tools
ifeq ($(strip $(BUILD_DIR)),)
$(error BUILD_DIR must not be empty)
endif
ifneq ($(words $(BUILD_DIR)),1)
$(error BUILD_DIR must not contain whitespace; use a relative path from a spaced checkout)
endif
ifeq ($(OS),Windows_NT)
SHELL := cmd.exe
EXEEXT := .exe
else
EXEEXT :=
endif
PROGRAM := $(BUILD_DIR)/bin2h$(EXEEXT)
.PHONY: all clean print-sources
all: $(PROGRAM)
$(BUILD_DIR):
ifeq ($(OS),Windows_NT)
	@if not exist "$(subst /,\,$@)" mkdir "$(subst /,\,$@)"
else
	mkdir -p "$@"
endif
$(PROGRAM): bin2h.c Makefile | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c11 $(LDFLAGS) "$<" $(LDLIBS) -o "$@"
print-sources:
	@echo bin2h.c
clean:
ifeq ($(OS),Windows_NT)
	@if exist "$(subst /,\,$(PROGRAM))" del /q "$(subst /,\,$(PROGRAM))"
else
	rm -f "$(PROGRAM)"
endif
