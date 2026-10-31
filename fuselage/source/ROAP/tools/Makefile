# Standalone SDK host tool. SEE: (ROAPPACK.md, Building the tool)
.DEFAULT_GOAL := all
.DELETE_ON_ERROR:
ifeq ($(origin CC),default)
CC := clang
endif
CC ?= clang
CFLAGS ?= -O2
BUILD_DIR ?= ../../../build/tools
FORMAT_INCLUDE_DIR ?= ..
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
SOURCES := roappack.c roappack_io.c roappack_pack.c roappack_list.c roappack_elf.c
OBJECTS := $(addprefix $(BUILD_DIR)/,$(SOURCES:.c=.o))
DEPENDENCIES := $(OBJECTS:.o=.d)
PROGRAM := $(BUILD_DIR)/roappack$(EXEEXT)
.PHONY: all clean print-sources
all: $(PROGRAM)
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
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c11 -I"$(FORMAT_INCLUDE_DIR)" -MMD -MP -c "$<" -o "$@"
$(PROGRAM): $(OBJECTS) Makefile
	$(CC) $(CFLAGS) $(LDFLAGS) $(foreach object,$(OBJECTS),"$(object)") $(LDLIBS) -o "$@"
print-sources:
	@echo $(SOURCES)
clean:
ifeq ($(OS),Windows_NT)
	@for %%f in ($(foreach output,$(OBJECTS) $(DEPENDENCIES) $(PROGRAM),"$(subst /,\,$(output))")) do @if exist "%%~f" del /q "%%~f"
else
	rm -f $(foreach output,$(OBJECTS) $(DEPENDENCIES) $(PROGRAM),"$(output)")
endif
-include $(DEPENDENCIES)
