# Standalone save storage. SEE: (SRAM.md, Building)
.DEFAULT_GOAL := all
.DELETE_ON_ERROR:
ifeq ($(origin CC),default)
CC := clang
endif
AR ?= ar
CFLAGS ?= -O2
BUILD_DIR ?= ../../build/SRAM/native
SOURCES := core/sram.c storage/sram_format.c storage/sram_paths.c storage/sram_platform.c storage/sram_file.c
OBJECTS := $(addprefix $(BUILD_DIR)/,$(SOURCES:.c=.o))
DEPENDENCIES := $(OBJECTS:.o=.d)
LIBRARY := $(BUILD_DIR)/libsram.a
ifeq ($(OS),Windows_NT)
SHELL := cmd.exe
endif
.PHONY: all print-libs
all: $(LIBRARY)
$(BUILD_DIR):
ifeq ($(OS),Windows_NT)
	@if not exist "$(subst /,\,$@)" mkdir "$(subst /,\,$@)"
else
	mkdir -p "$@"
endif
$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
ifeq ($(OS),Windows_NT)
	@if not exist "$(subst /,\,$(@D))" mkdir "$(subst /,\,$(@D))"
else
	mkdir -p "$(@D)"
endif
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c11 -MMD -MP -c "$<" -o "$@"
$(LIBRARY): $(OBJECTS)
	$(AR) rcs "$@" $(OBJECTS)
print-libs:
	@echo No additional link libraries.
-include $(DEPENDENCIES)
