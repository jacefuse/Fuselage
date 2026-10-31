# Standalone execution core. SEE: (VIVIAN.md, Building VIVIAN)
.DEFAULT_GOAL := all
.DELETE_ON_ERROR:

ifeq ($(origin CC),default)
CC := clang
endif
CC ?= clang
AR ?= ar
CFLAGS ?= -O2

TARGET_TRIPLE := $(shell $(CC) -dumpmachine)
PLATFORM ?= $(if $(findstring mingw,$(TARGET_TRIPLE)),windows,$(if $(findstring windows,$(TARGET_TRIPLE)),windows,$(if $(findstring darwin,$(TARGET_TRIPLE)),macos,$(if $(findstring linux,$(TARGET_TRIPLE)),linux,unknown))))
ifeq ($(filter $(PLATFORM),windows macos linux),)
$(error Unsupported compiler target '$(TARGET_TRIPLE)'; set PLATFORM to windows, linux or macos)
endif
BUILD_DIR ?= ../../build/VIVIAN/$(PLATFORM)
ifeq ($(strip $(BUILD_DIR)),)
$(error BUILD_DIR must not be empty)
endif
ifneq ($(words $(BUILD_DIR)),1)
$(error BUILD_DIR must not contain whitespace; use a relative path from a spaced checkout)
endif

SOURCES := core/vivian_state.c memory/vivian_memory.c floating/vivian_float_environment.c \
           compressed/vivian_compressed.c execution/vivian_execute.c
VIVIAN_CFLAGS := -std=c11
VIVIAN_LDLIBS ?= -lm
OBJECTS := $(addprefix $(BUILD_DIR)/,$(SOURCES:.c=.o))
DEPENDENCIES := $(OBJECTS:.o=.d)
LIBRARY := $(BUILD_DIR)/libvivian.a

ifeq ($(OS),Windows_NT)
SHELL := cmd.exe
endif

.PHONY: all clean print-libs print-sources
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
	$(CC) $(CPPFLAGS) $(CFLAGS) $(VIVIAN_CFLAGS) -MMD -MP -c "$<" -o "$@"

$(LIBRARY): $(OBJECTS) Makefile
ifeq ($(OS),Windows_NT)
	@if exist "$(subst /,\,$@)" del /q "$(subst /,\,$@)"
else
	rm -f "$@"
endif
	$(AR) rcs "$@" $(foreach object,$(OBJECTS),"$(object)")

# Place these dependencies after the archive on the application link command.
print-libs:
	@echo $(VIVIAN_LDLIBS)

print-sources:
	@echo $(SOURCES)

# Remove only listed outputs, retaining unrelated files and the directory.
clean:
ifeq ($(OS),Windows_NT)
	@for %%f in ($(foreach output,$(OBJECTS) $(DEPENDENCIES) $(LIBRARY),"$(subst /,\,$(output))")) do @if exist "%%~f" del /q "%%~f"
else
	rm -f $(foreach output,$(OBJECTS) $(DEPENDENCIES) $(LIBRARY),"$(output)")
endif

-include $(DEPENDENCIES)
