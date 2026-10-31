# Standalone SHARP library. SEE: (SHARP.md, Building SHARP)
.DEFAULT_GOAL := all
.DELETE_ON_ERROR:

ifeq ($(origin CC),default)
CC := clang
endif
CC ?= clang
AR ?= ar
CFLAGS ?= -O2
PKG_CONFIG ?= pkg-config

TARGET_TRIPLE := $(shell $(CC) -dumpmachine)
PLATFORM ?= $(if $(findstring mingw,$(TARGET_TRIPLE)),windows,$(if $(findstring windows,$(TARGET_TRIPLE)),windows,$(if $(findstring darwin,$(TARGET_TRIPLE)),macos,$(if $(findstring linux,$(TARGET_TRIPLE)),linux,unknown))))
BUILD_DIR ?= ../../build/SHARP/$(PLATFORM)
ifeq ($(strip $(BUILD_DIR)),)
$(error BUILD_DIR must not be empty)
endif

COMMON_SOURCES := core/sharp.c core/sharp_pool.c core/sharp_chips.c core/sharp_mixer.c core/sharp_capture.c \
                  backend/sharp_backend.c transport/sharp_events.c transport/sharp_ring.c mixer/lori.c \
                  chips/adora/adora.c chips/adora/adora_voice.c chips/adora/adora_registers.c chips/adora/adora_render.c \
                  chips/sweeny/sweeny.c chips/sweeny/sweeny_voice.c chips/sweeny/sweeny_registers.c chips/sweeny/sweeny_render.c \
                  chips/amy/amy.c chips/amy/amy_volume.c chips/amy/amy_envelope.c chips/amy/amy_registers.c chips/amy/amy_render.c \
                  chips/jenny/jenny.c chips/jenny/jenny_voice.c chips/jenny/jenny_registers.c chips/jenny/jenny_render.c
SHARP_CFLAGS := -std=c11
SHARP_CPPFLAGS =

ifeq ($(PLATFORM),windows)
PLATFORM_SOURCES := platform/windows/sharp_wasapi.c platform/windows/sharp_wasapi_stream.c platform/windows/sharp_wasapi_format.c platform/windows/sharp_thread_windows.c
SHARP_LDLIBS := -lole32 -lm
else ifeq ($(PLATFORM),macos)
PLATFORM_SOURCES := platform/macos/sharp_coreaudio.c platform/macos/sharp_coreaudio_stream.c platform/posix/sharp_thread_posix.c
SHARP_CFLAGS += -pthread
SHARP_LDLIBS := -framework AudioToolbox -framework CoreAudio -pthread -lm
else ifeq ($(PLATFORM),linux)
PLATFORM_SOURCES := platform/linux/sharp_alsa.c platform/linux/sharp_alsa_stream.c platform/linux/sharp_alsa_format.c platform/posix/sharp_thread_posix.c
ALSA_CFLAGS ?= $(shell $(PKG_CONFIG) --cflags alsa)
ALSA_LIBS ?= $(shell $(PKG_CONFIG) --libs alsa)
SHARP_CPPFLAGS = $(ALSA_CFLAGS)
SHARP_CFLAGS += -pthread
SHARP_LDLIBS = $(ALSA_LIBS) -pthread -lm
else
$(error Unsupported compiler target '$(TARGET_TRIPLE)'; set PLATFORM to windows, linux or macos)
endif

SOURCES := $(COMMON_SOURCES) $(PLATFORM_SOURCES)
OBJECTS := $(addprefix $(BUILD_DIR)/,$(SOURCES:.c=.o))
DEPENDENCIES := $(OBJECTS:.o=.d)
LIBRARY := $(BUILD_DIR)/libsharp.a

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
	$(CC) $(CPPFLAGS) $(SHARP_CPPFLAGS) $(CFLAGS) $(SHARP_CFLAGS) -MMD -MP -c "$<" -o "$@"

$(LIBRARY): $(OBJECTS)
ifeq ($(OS),Windows_NT)
	@if exist "$(subst /,\,$@)" del /q "$(subst /,\,$@)"
else
	rm -f "$@"
endif
	$(AR) rcs "$@" $(foreach object,$(OBJECTS),"$(object)")

# Place these dependencies after the archive on the application link command.
print-libs:
	@echo $(SHARP_LDLIBS)

print-sources:
	@echo $(SOURCES)

# Remove only this platform build's listed outputs, retaining unrelated files.
clean:
ifeq ($(OS),Windows_NT)
	@for %%f in ($(foreach output,$(OBJECTS) $(DEPENDENCIES) $(LIBRARY),"$(subst /,\,$(output))")) do @if exist "%%~f" del /q "%%~f"
else
	rm -f $(foreach output,$(OBJECTS) $(DEPENDENCIES) $(LIBRARY),"$(output)")
endif

-include $(DEPENDENCIES)
