# Standalone CAKE library. SEE: (CAKE.md, Building CAKE)
.DEFAULT_GOAL := all
.DELETE_ON_ERROR:

ifeq ($(origin CC),default)
CC := clang
endif
ifeq ($(origin OBJC),default)
OBJC := $(CC)
endif
CC ?= clang
OBJC ?= $(CC)
AR ?= ar
CFLAGS ?= -O2
OBJCFLAGS ?= $(CFLAGS)
PKG_CONFIG ?= pkg-config

TARGET_TRIPLE := $(shell $(CC) -dumpmachine)
PLATFORM ?= $(if $(findstring mingw,$(TARGET_TRIPLE)),windows,$(if $(findstring windows,$(TARGET_TRIPLE)),windows,$(if $(findstring darwin,$(TARGET_TRIPLE)),macos,$(if $(findstring linux,$(TARGET_TRIPLE)),linux,unknown))))
BUILD_DIR ?= ../../build/CAKE/$(PLATFORM)

C_SOURCES := core/cake_state.c helpers/cake_help.c helpers/cake_labels.c helpers/cake_repeat.c
OBJC_SOURCES :=
CAKE_CFLAGS := -std=c11
CAKE_CPPFLAGS :=
GENERATED :=

ifeq ($(PLATFORM),windows)
C_SOURCES += platform/windows/cake_win32_window.c platform/windows/cake_win32_input.c platform/windows/cake_win32_raw_input.c \
             platform/windows/cake_win32_controllers.c platform/windows/cake_win32_discovery.c platform/windows/cake_win32_xinput.c \
             platform/windows/cake_win32_hid_layouts.c platform/windows/cake_win32_hid_binding.c platform/windows/cake_win32_hid_reports.c
CAKE_LDLIBS := -luser32 -lhid -lsetupapi
else ifeq ($(PLATFORM),macos)
C_SOURCES += platform/macos/cake_macos_input.c platform/macos/cake_macos_controllers.c platform/macos/cake_macos_lifecycle.c
OBJC_SOURCES += platform/macos/cake_macos_events.m platform/macos/cake_macos_gamecontroller.m
CAKE_CFLAGS += -pthread
CAKE_LDLIBS := -framework Cocoa -framework GameController -pthread
else ifeq ($(PLATFORM),linux)
C_SOURCES += platform/linux/cake_linux_input.c platform/linux/cake_linux_wayland.c platform/linux/cake_linux_x11.c \
             platform/linux/cake_linux_lifecycle.c platform/linux/cake_linux_controller_events.c \
             platform/linux/cake_linux_discovery.c platform/linux/cake_linux_controllers.c
LINUX_CFLAGS ?= $(shell $(PKG_CONFIG) --cflags wayland-client x11 xi)
LINUX_LIBS ?= $(shell $(PKG_CONFIG) --libs wayland-client x11 xi)
WAYLAND_PROTOCOLS_DIR ?= $(shell $(PKG_CONFIG) --variable=pkgdatadir wayland-protocols)
WAYLAND_SCANNER ?= $(shell $(PKG_CONFIG) --variable=wayland_scanner wayland-scanner)
RELATIVE_POINTER_XML ?= $(WAYLAND_PROTOCOLS_DIR)/unstable/relative-pointer/relative-pointer-unstable-v1.xml
PROTOCOL_HEADER := $(BUILD_DIR)/relative-pointer-unstable-v1-client-protocol.h
PROTOCOL_SOURCE := $(BUILD_DIR)/relative-pointer-unstable-v1-protocol.c
GENERATED := $(PROTOCOL_HEADER) $(PROTOCOL_SOURCE)
CAKE_CPPFLAGS += -I"$(BUILD_DIR)" $(LINUX_CFLAGS)
CAKE_CFLAGS += -pthread
CAKE_LDLIBS = $(LINUX_LIBS) -pthread -lm
else
$(error Unsupported compiler target '$(TARGET_TRIPLE)'; set PLATFORM to windows, linux or macos)
endif

OBJECTS := $(addprefix $(BUILD_DIR)/,$(C_SOURCES:.c=.o) $(OBJC_SOURCES:.m=.o))
ifeq ($(PLATFORM),linux)
OBJECTS += $(BUILD_DIR)/relative-pointer-unstable-v1-protocol.o
endif
DEPENDENCIES := $(OBJECTS:.o=.d)
LIBRARY := $(BUILD_DIR)/libcake.a

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
	$(CC) $(CPPFLAGS) $(CAKE_CPPFLAGS) $(CFLAGS) $(CAKE_CFLAGS) -MMD -MP -c "$<" -o "$@"

$(BUILD_DIR)/%.o: %.m Makefile | $(BUILD_DIR)
ifeq ($(OS),Windows_NT)
	@if not exist "$(subst /,\,$(@D))" mkdir "$(subst /,\,$(@D))"
else
	mkdir -p "$(@D)"
endif
	$(OBJC) $(CPPFLAGS) $(OBJCFLAGS) -fobjc-arc -MMD -MP -c "$<" -o "$@"

ifeq ($(PLATFORM),linux)
$(BUILD_DIR)/platform/linux/cake_linux_wayland.o: $(PROTOCOL_HEADER)

$(PROTOCOL_HEADER): $(RELATIVE_POINTER_XML) Makefile | $(BUILD_DIR)
	$(WAYLAND_SCANNER) client-header "$<" "$@"

$(PROTOCOL_SOURCE): $(RELATIVE_POINTER_XML) Makefile | $(BUILD_DIR)
	$(WAYLAND_SCANNER) private-code "$<" "$@"

$(BUILD_DIR)/relative-pointer-unstable-v1-protocol.o: $(PROTOCOL_SOURCE) $(PROTOCOL_HEADER) Makefile
	$(CC) $(CPPFLAGS) $(CAKE_CPPFLAGS) $(CFLAGS) $(CAKE_CFLAGS) -MMD -MP -c "$<" -o "$@"
endif

$(LIBRARY): $(OBJECTS)
ifeq ($(OS),Windows_NT)
	@if exist "$(subst /,\,$@)" del /q "$(subst /,\,$@)"
else
	rm -f "$@"
endif
	$(AR) rcs "$@" $(OBJECTS)

# These dependencies belong after the archive on the final application link.
print-libs:
	@echo $(CAKE_LDLIBS)

# Remove only this build's listed outputs, retaining unrelated files.
clean:
ifeq ($(OS),Windows_NT)
	@for %%f in ($(subst /,\,$(OBJECTS) $(DEPENDENCIES) $(LIBRARY) $(GENERATED))) do @if exist "%%f" del /q "%%f"
else
	rm -f $(OBJECTS) $(DEPENDENCIES) "$(LIBRARY)" $(GENERATED)
endif

-include $(DEPENDENCIES)
