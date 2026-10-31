# Standalone graphics archive. SEE: (FUSELAGE_BUILD.md, Dependencies)
.DEFAULT_GOAL := all
.DELETE_ON_ERROR:
ifeq ($(origin CC),default)
CC := clang
endif
CC ?= clang
OBJC = $(CC)
AR ?= ar
CFLAGS ?= -O2
OBJCFLAGS ?= $(CFLAGS)
PKG_CONFIG ?= pkg-config
TARGET_TRIPLE := $(shell $(CC) -dumpmachine)
PLATFORM ?= $(if $(findstring mingw,$(TARGET_TRIPLE)),windows,$(if $(findstring windows,$(TARGET_TRIPLE)),windows,$(if $(findstring darwin,$(TARGET_TRIPLE)),macos,$(if $(findstring linux,$(TARGET_TRIPLE)),linux,unknown))))
BUILD_DIR ?= ../../build/GDMF/$(PLATFORM)
include sources.mk
GDMF_CPPFLAGS := -I. -I.. -Ivulkan -Icompute -Ipixies -Isprites -Itiles -Itextlayer
GDMF_CFLAGS := -std=c11 -Wall -Wextra -Wpedantic
VULKAN_INCLUDE_DIR ?= $(VULKAN_SDK)/$(if $(filter windows,$(PLATFORM)),Include,include)
VULKAN_CFLAGS ?= $(if $(VULKAN_SDK),-I"$(VULKAN_INCLUDE_DIR)",$(shell $(PKG_CONFIG) --cflags vulkan))
GDMF_CPPFLAGS += $(VULKAN_CFLAGS)
GENERATED_OBJECTS :=
ifeq ($(PLATFORM),windows)
SOURCES := $(GDMF_COMMON_SOURCES) $(GDMF_WIN32_SOURCES)
ifneq ($(findstring clang,$(shell $(CC) --version)),)
GDMF_CFLAGS += -fno-emulated-tls
endif
else ifeq ($(PLATFORM),macos)
SOURCES := $(GDMF_COMMON_SOURCES) $(GDMF_MACOS_SOURCES)
GDMF_CFLAGS += -pthread
else ifeq ($(PLATFORM),linux)
SOURCES := $(GDMF_COMMON_SOURCES) $(GDMF_LINUX_SOURCES)
GDMF_CFLAGS += -pthread
LINUX_CFLAGS ?= $(shell $(PKG_CONFIG) --cflags wayland-client wayland-cursor x11 xrandr xscrnsaver)
GDMF_CPPFLAGS += $(LINUX_CFLAGS) -I"$(BUILD_DIR)"
WAYLAND_PROTOCOLS_DIR ?= $(shell $(PKG_CONFIG) --variable=pkgdatadir wayland-protocols)
WAYLAND_SCANNER ?= wayland-scanner
PROTOCOL_NAMES := xdg-shell xdg-decoration-unstable-v1 pointer-constraints-unstable-v1 idle-inhibit-unstable-v1
xdg-shell_XML := $(WAYLAND_PROTOCOLS_DIR)/stable/xdg-shell/xdg-shell.xml
xdg-decoration-unstable-v1_XML := $(WAYLAND_PROTOCOLS_DIR)/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml
pointer-constraints-unstable-v1_XML := $(WAYLAND_PROTOCOLS_DIR)/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml
idle-inhibit-unstable-v1_XML := $(WAYLAND_PROTOCOLS_DIR)/unstable/idle-inhibit/idle-inhibit-unstable-v1.xml
PROTOCOL_HEADERS := $(addprefix $(BUILD_DIR)/,$(addsuffix -client-protocol.h,$(PROTOCOL_NAMES)))
GENERATED_OBJECTS := $(addprefix $(BUILD_DIR)/,$(addsuffix -protocol.o,$(PROTOCOL_NAMES)))
define protocol_rules
$(BUILD_DIR)/$(1)-client-protocol.h: $$($(1)_XML) | $(BUILD_DIR)
	$(WAYLAND_SCANNER) client-header "$$<" "$$@"
$(BUILD_DIR)/$(1)-protocol.c: $$($(1)_XML) | $(BUILD_DIR)
	$(WAYLAND_SCANNER) private-code "$$<" "$$@"
endef
$(foreach protocol,$(PROTOCOL_NAMES),$(eval $(call protocol_rules,$(protocol))))
else
$(error Unsupported target '$(TARGET_TRIPLE)'; set PLATFORM to windows, macos or linux)
endif
ifeq ($(strip $(BUILD_DIR)),)
$(error BUILD_DIR must not be empty)
endif
ifneq ($(words $(BUILD_DIR)),1)
$(error Use a whitespace-free relative BUILD_DIR, even in a spaced checkout)
endif
OBJECTS := $(addprefix $(BUILD_DIR)/,$(addsuffix .o,$(basename $(SOURCES)))) $(GENERATED_OBJECTS)
LIBRARY := $(BUILD_DIR)/libgdmf.a
ifeq ($(OS),Windows_NT)
SHELL := cmd.exe
endif
.PHONY: all print-sources
all: $(LIBRARY)
$(BUILD_DIR):
ifeq ($(OS),Windows_NT)
	@if not exist "$(subst /,\,$@)" mkdir "$(subst /,\,$@)"
else
	mkdir -p "$@"
endif
$(BUILD_DIR)/%.o: %.c Makefile sources.mk $(PROTOCOL_HEADERS) | $(BUILD_DIR)
ifeq ($(OS),Windows_NT)
	@if not exist "$(subst /,\,$(@D))" mkdir "$(subst /,\,$(@D))"
else
	mkdir -p "$(@D)"
endif
	$(CC) $(CPPFLAGS) $(GDMF_CPPFLAGS) $(CFLAGS) $(GDMF_CFLAGS) -MMD -MP -c "$<" -o "$@"
$(BUILD_DIR)/%.o: %.m Makefile sources.mk | $(BUILD_DIR)
ifeq ($(OS),Windows_NT)
	@if not exist "$(subst /,\,$(@D))" mkdir "$(subst /,\,$(@D))"
else
	mkdir -p "$(@D)"
endif
	mkdir -p "$(@D)"
	$(OBJC) $(CPPFLAGS) $(GDMF_CPPFLAGS) $(OBJCFLAGS) -MMD -MP -c "$<" -o "$@"
$(BUILD_DIR)/%-protocol.o: $(BUILD_DIR)/%-protocol.c $(PROTOCOL_HEADERS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(GDMF_CPPFLAGS) $(CFLAGS) $(GDMF_CFLAGS) -MMD -MP -c "$<" -o "$@"
$(LIBRARY): $(OBJECTS)
	$(if $(findstring n,$(filter-out --%,$(firstword $(MAKEFLAGS)))),,$(file >$(BUILD_DIR)/objects.rsp,$(foreach object,$(OBJECTS),"$(object)")))
ifeq ($(OS),Windows_NT)
	@if exist "$(subst /,\,$@.tmp)" del /q "$(subst /,\,$@.tmp)"
else
	rm -f "$@.tmp"
endif
	$(AR) rcs "$@.tmp" "@$(BUILD_DIR)/objects.rsp"
ifeq ($(OS),Windows_NT)
	move /Y "$(subst /,\,$@.tmp)" "$(subst /,\,$@)" >NUL
else
	mv -f "$@.tmp" "$@"
endif
print-sources:
	@echo $(SOURCES)
-include $(OBJECTS:.o=.d)
