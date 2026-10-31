# Standalone DICE library. SEE: (DICE.md, Building DICE)
.DEFAULT_GOAL := all
.DELETE_ON_ERROR:

ifeq ($(origin CC),default)
CC := clang
endif
AR ?= ar
CFLAGS ?= -O2

TARGET_TRIPLE := $(shell $(CC) -dumpmachine)
COMPILER_VERSION := $(shell $(CC) --version)
PLATFORM ?= $(if $(findstring mingw,$(TARGET_TRIPLE)),windows,$(if $(findstring windows,$(TARGET_TRIPLE)),windows,$(if $(findstring darwin,$(TARGET_TRIPLE)),macos,$(if $(findstring linux,$(TARGET_TRIPLE)),linux,unknown))))
BUILD_DIR ?= ../../build/DICE/$(PLATFORM)

SOURCES := core/dice.c clock/dice_elapsed.c clock/dice_calendar.c rng/dice_rng.c rng/dice_rng_entropy.c \
           helpers/dice_help.c helpers/dice_sequences.c timers/dice_timers.c
DICE_CFLAGS := -std=c11

ifeq ($(PLATFORM),windows)
SOURCES += timers/dice_timers_win32.c
DICE_LDLIBS := -lbcrypt
ifneq ($(findstring clang,$(COMPILER_VERSION)),)
DICE_CFLAGS += -fno-emulated-tls
endif
else ifneq ($(filter $(PLATFORM),linux macos),)
SOURCES += timers/dice_timers_posix.c
DICE_CFLAGS += -pthread
DICE_LDLIBS := -pthread -lm
else
$(error Unsupported compiler target '$(TARGET_TRIPLE)'; set PLATFORM to windows, linux or macos)
endif

OBJECTS := $(addprefix $(BUILD_DIR)/,$(SOURCES:.c=.o))
DEPENDENCIES := $(OBJECTS:.o=.d)
LIBRARY := $(BUILD_DIR)/libdice.a

# Native Windows make uses cmd; POSIX hosts use make's default shell.
ifeq ($(OS),Windows_NT)
SHELL := cmd.exe
endif

.PHONY: all clean print-libs
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
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DICE_CFLAGS) -MMD -MP -c "$<" -o "$@"

$(LIBRARY): $(OBJECTS)
ifeq ($(OS),Windows_NT)
	@if exist "$(subst /,\,$@)" del /q "$(subst /,\,$@)"
else
	rm -f "$@"
endif
	$(AR) rcs "$@" $(OBJECTS)

# Link applications with the archive followed by these native dependencies.
print-libs:
	@echo $(DICE_LDLIBS)

# Remove only this build's listed outputs; retain the directory and other files.
clean:
ifeq ($(OS),Windows_NT)
	@for %%f in ($(subst /,\,$(OBJECTS) $(DEPENDENCIES) $(LIBRARY))) do @if exist "%%f" del /q "%%f"
else
	rm -f $(OBJECTS) $(DEPENDENCIES) "$(LIBRARY)"
endif

-include $(DEPENDENCIES)
