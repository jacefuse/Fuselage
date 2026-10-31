# Native engine programs. SEE: (FUSELAGE_BUILD.md, Building)
.DEFAULT_GOAL := all
.DELETE_ON_ERROR:
ifeq ($(origin CC),default)
CC := clang
endif
CC ?= clang
AR ?= ar
REQUESTED_CONFIG := $(filter release debug,$(MAKECMDGOALS))
ifneq ($(word 2,$(REQUESTED_CONFIG)),)
$(error Select either release or debug, not both)
endif
CONFIG ?= $(if $(REQUESTED_CONFIG),$(REQUESTED_CONFIG),release)
ifneq ($(REQUESTED_CONFIG),)
ifneq ($(CONFIG),$(REQUESTED_CONFIG))
$(error CONFIG conflicts with the requested release/debug goal)
endif
endif
ifeq ($(filter $(CONFIG),release debug),)
$(error CONFIG must be release or debug)
endif
CFLAGS ?= $(if $(filter debug,$(CONFIG)),-O0 -g -DDEBUG,-O2 -DNDEBUG)
PKG_CONFIG ?= pkg-config
TARGET_TRIPLE := $(shell $(CC) -dumpmachine)
PLATFORM ?= $(if $(findstring mingw,$(TARGET_TRIPLE)),windows,$(if $(findstring windows,$(TARGET_TRIPLE)),windows,$(if $(findstring darwin,$(TARGET_TRIPLE)),macos,$(if $(findstring linux,$(TARGET_TRIPLE)),linux,unknown))))
REQUESTED_MODE := $(filter launcher native embedded,$(MAKECMDGOALS))
ifneq ($(word 2,$(REQUESTED_MODE)),)
$(error Select only one executable mode)
endif
MODE ?= $(if $(REQUESTED_MODE),$(REQUESTED_MODE),default)
ifeq ($(filter $(MODE),default launcher native embedded),)
$(error MODE must be default, launcher, native or embedded)
endif
BUILD_DIR ?= build/obj/$(PLATFORM)/$(MODE)/$(CONFIG)
ifeq ($(strip $(BUILD_DIR)),)
$(error BUILD_DIR must not be empty)
endif
ifneq ($(words $(BUILD_DIR)),1)
$(error Use a whitespace-free relative BUILD_DIR, even in a spaced checkout)
endif
ifneq ($(filter /%,$(BUILD_DIR))$(findstring :,$(BUILD_DIR)),)
$(error BUILD_DIR must be relative to the fuselage directory)
endif
EXEEXT := $(if $(filter windows,$(PLATFORM)),.exe,)
WINDOWS_RESOURCES :=
ifeq ($(PLATFORM),windows)
ifeq ($(MODE),launcher)
ifneq ($(BUILD_LIBRARY),1)
WINDRES ?= windres
WINDOWS_RESOURCES := $(BUILD_DIR)/launcher_icon.o
$(BUILD_DIR)/launcher_icon.o: source/BASE/application/windows/launcher.rc ../assets/RocketGirlRemake.ico | $(BUILD_DIR)
	$(WINDRES) -I.. -i "$<" -O coff -o "$@"
endif
endif
endif
OUTPUT ?= build/$(if $(filter launcher,$(MODE)),MISLlauncher,fuselage)$(if $(filter debug,$(CONFIG)),-debug,)$(EXEEXT)
ifeq ($(MODE),default)
PROJECT_DIR := .
endif
include source/MISL/host/sources.mk
ROOT_SOURCES := source/BASE/core/fuselage.c source/BASE/core/fuselage_state.c source/BASE/core/fuselage_controls.c \
 source/BASE/core/fuselage_callbacks.c source/BASE/core/fuselage_signals.c source/BASE/application/fuselage_banner.c \
 source/BASE/core/fuselage_lifecycle.c source/BASE/core/fuselage_platform.c source/BASE/application/fuselage_save_ui.c \
 source/BASE/core/fuselage_versions.c source/BASE/pacing/fuselage_pacing.c source/BASE/pacing/fuselage_pacing_estimator.c \
 source/BASE/pacing/fuselage_pacing_trace.c source/BASE/application/main.c
SOURCES := $(ROOT_SOURCES) $(MISL_HOST_SOURCES) source/MISL/format/misl_format_append.c \
 source/MISL/math/misl_math_trig.c source/MISL/math/misl_math_exponential.c source/MISL/math/misl_math_power.c \
 source/MISL/math/misl_math_round.c source/MISL/math/misl_math_float.c
ENGINE_CPPFLAGS := -Isource -Isource/MISL/host
ENGINE_CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -ffp-contract=off
ifeq ($(MODE),launcher)
SOURCES += $(MISL_LAUNCHER_SOURCES)
ENGINE_CPPFLAGS += -DFUSELAGE_ROAP_LAUNCHER
else
ifeq ($(strip $(PROJECT_DIR)),)
$(error PROJECT_DIR must name the directory containing project_config.h)
endif
ifneq ($(words $(PROJECT_DIR)),1)
$(error Use a whitespace-free relative PROJECT_DIR, even in a spaced checkout)
endif
ENGINE_CPPFLAGS += -I"$(PROJECT_DIR)"
ifneq ($(filter $(MODE),native default),)
PROJECT_SOURCES ?= $(PROJECT_DIR)/project.c
else
ifeq ($(strip $(ROAP_FILE)),)
$(error ROAP_FILE must name the prebuilt ROAP to embed)
endif
SOURCES += source/MISL/host/runtime/misl_embedded.c
ENGINE_CPPFLAGS += -DFUSELAGE_EMBEDDED_ROAP -I"$(BUILD_DIR)/generated"
endif
endif
SUBSYSTEMS := GDMF CAKE DICE SHARP SRAM VIVIAN
# Explicit names keep native Windows make independent of POSIX shell utilities.
LIBRARIES := $(BUILD_DIR)/GDMF/libgdmf.a $(BUILD_DIR)/CAKE/libcake.a $(BUILD_DIR)/DICE/libdice.a \
 $(BUILD_DIR)/SHARP/libsharp.a $(BUILD_DIR)/SRAM/libsram.a $(BUILD_DIR)/VIVIAN/libvivian.a
VULKAN_LIBRARY_DIR ?= $(VULKAN_SDK)/$(if $(filter windows,$(PLATFORM)),Lib,lib)
VULKAN_LIBS ?= $(if $(VULKAN_SDK),-L"$(VULKAN_LIBRARY_DIR)" -l$(if $(filter windows,$(PLATFORM)),vulkan-1,vulkan),$(shell $(PKG_CONFIG) --libs vulkan))
ifeq ($(PLATFORM),windows)
ifneq ($(findstring clang,$(shell $(CC) --version)),)
ENGINE_CFLAGS += -fno-emulated-tls
endif
WINDOWS_SUBSYSTEM ?= $(if $(filter debug,$(CONFIG)),console,windows)
ifeq ($(WINDOWS_SUBSYSTEM),windows)
ENGINE_LDFLAGS := -mwindows
else ifneq ($(WINDOWS_SUBSYSTEM),console)
$(error WINDOWS_SUBSYSTEM must be console or windows)
endif
ifeq ($(CONFIG),release)
ENGINE_LDFLAGS += -s
endif
PLATFORM_LIBS := $(VULKAN_LIBS) -luser32 -lgdi32 -lhid -lsetupapi -lbcrypt -lole32 -lm
else ifeq ($(PLATFORM),macos)
ENGINE_CFLAGS += -pthread
PLATFORM_LIBS := $(VULKAN_LIBS) -framework Cocoa -framework QuartzCore -framework IOKit \
 -framework CoreFoundation -framework GameController -framework AudioToolbox -framework CoreAudio -pthread -lm
else ifeq ($(PLATFORM),linux)
ENGINE_CFLAGS += -pthread
PLATFORM_LIBS := $(VULKAN_LIBS) $(shell $(PKG_CONFIG) --libs wayland-client wayland-cursor x11 xi xrandr xscrnsaver alsa) -pthread -lm
else
$(error Unsupported compiler target '$(TARGET_TRIPLE)')
endif
OBJECTS := $(addprefix $(BUILD_DIR)/,$(SOURCES:.c=.o))
PROJECT_OBJECTS := $(foreach source,$(PROJECT_SOURCES),$(BUILD_DIR)/project/$(notdir $(source:.c=.o)))
ifneq ($(words $(PROJECT_OBJECTS)),$(words $(sort $(PROJECT_OBJECTS))))
$(error PROJECT_SOURCES must have unique basenames)
endif
ifeq ($(OS),Windows_NT)
SHELL := cmd.exe
endif
.PHONY: all subsystems release debug launcher native embedded lib library-internal
release debug native embedded: all
all: $(OUTPUT)
ifeq ($(MODE),default)
all: lib
endif
ifeq ($(MODE),launcher)
launcher: all
else
launcher:
	$(MAKE) MODE=launcher CONFIG=$(CONFIG) BUILD_DIR=build/obj/$(PLATFORM)/launcher/$(CONFIG) OUTPUT=build/MISLlauncher$(if $(filter debug,$(CONFIG)),-debug,)$(EXEEXT) all
endif
lib:
	$(MAKE) MODE=launcher CONFIG=$(CONFIG) BUILD_LIBRARY=1 BUILD_DIR=build/obj/$(PLATFORM)/library/$(CONFIG) OUTPUT=build/libfuselage-$(CONFIG).a library-internal
library-internal: $(OUTPUT)
$(BUILD_DIR):
ifeq ($(OS),Windows_NT)
	@if not exist "$(subst /,\,$@)" mkdir "$(subst /,\,$@)"
else
	mkdir -p "$@"
endif
subsystems: | $(BUILD_DIR)
	$(MAKE) -C source/GDMF CC="$(CC)" AR="$(AR)" CFLAGS="$(CFLAGS)" PLATFORM=$(PLATFORM) BUILD_DIR="../../$(BUILD_DIR)/GDMF"
	$(MAKE) -C source/CAKE CC="$(CC)" AR="$(AR)" CFLAGS="$(CFLAGS)" PLATFORM=$(PLATFORM) BUILD_DIR="../../$(BUILD_DIR)/CAKE"
	$(MAKE) -C source/DICE CC="$(CC)" AR="$(AR)" CFLAGS="$(CFLAGS)" PLATFORM=$(PLATFORM) BUILD_DIR="../../$(BUILD_DIR)/DICE"
	$(MAKE) -C source/SHARP CC="$(CC)" AR="$(AR)" CFLAGS="$(CFLAGS)" PLATFORM=$(PLATFORM) BUILD_DIR="../../$(BUILD_DIR)/SHARP"
	$(MAKE) -C source/SRAM CC="$(CC)" AR="$(AR)" CFLAGS="$(CFLAGS)" BUILD_DIR="../../$(BUILD_DIR)/SRAM"
	$(MAKE) -C source/VIVIAN CC="$(CC)" AR="$(AR)" CFLAGS="$(CFLAGS)" PLATFORM=$(PLATFORM) BUILD_DIR="../../$(BUILD_DIR)/VIVIAN"
$(OBJECTS): | subsystems
$(BUILD_DIR)/%.o: %.c Makefile source/MISL/host/sources.mk | $(BUILD_DIR)
ifeq ($(OS),Windows_NT)
	@if not exist "$(subst /,\,$(@D))" mkdir "$(subst /,\,$(@D))"
else
	mkdir -p "$(@D)"
endif
	$(CC) $(CPPFLAGS) $(ENGINE_CPPFLAGS) $(CFLAGS) $(ENGINE_CFLAGS) -MMD -MP -c "$<" -o "$@"
define project_rule
$(BUILD_DIR)/project/$(notdir $(1:.c=.o)): $(1) Makefile | $(BUILD_DIR)
ifeq ($(OS),Windows_NT)
	@if not exist "$(subst /,\,$(BUILD_DIR)/project)" mkdir "$(subst /,\,$(BUILD_DIR)/project)"
else
	mkdir -p "$(BUILD_DIR)/project"
endif
	$(CC) $(CPPFLAGS) $(ENGINE_CPPFLAGS) $(CFLAGS) $(ENGINE_CFLAGS) -MMD -MP -c "$$<" -o "$$@"
endef
$(foreach source,$(PROJECT_SOURCES),$(eval $(call project_rule,$(source))))
$(LIBRARIES): | subsystems

ifeq ($(BUILD_LIBRARY),1)
LIBRARY_OBJECTS := $(filter-out $(BUILD_DIR)/source/BASE/application/main.o,$(OBJECTS))
$(OUTPUT): $(LIBRARY_OBJECTS) $(LIBRARIES) | subsystems
	$(if $(findstring n,$(filter-out --%,$(firstword $(MAKEFLAGS)))),,$(file >$(BUILD_DIR)/archive.mri,CREATE $(OUTPUT).tmp)$(foreach object,$(LIBRARY_OBJECTS),$(file >>$(BUILD_DIR)/archive.mri,ADDMOD $(object)))$(foreach library,$(LIBRARIES),$(file >>$(BUILD_DIR)/archive.mri,ADDLIB $(library)))$(file >>$(BUILD_DIR)/archive.mri,SAVE)$(file >>$(BUILD_DIR)/archive.mri,END))
	$(AR) -M < "$(BUILD_DIR)/archive.mri"
ifeq ($(OS),Windows_NT)
	move /Y "$(subst /,\,$@.tmp)" "$(subst /,\,$@)" >NUL
else
	mv "$@.tmp" "$@"
endif
else
$(OUTPUT): $(OBJECTS) $(PROJECT_OBJECTS) $(WINDOWS_RESOURCES) $(LIBRARIES) | subsystems
	$(if $(findstring n,$(filter-out --%,$(firstword $(MAKEFLAGS)))),,$(file >$(BUILD_DIR)/link.rsp,$(foreach object,$(OBJECTS) $(PROJECT_OBJECTS) $(WINDOWS_RESOURCES) $(LIBRARIES),"$(object)")))
	$(CC) $(LDFLAGS) $(ENGINE_LDFLAGS) "@$(BUILD_DIR)/link.rsp" $(PLATFORM_LIBS) $(LDLIBS) -o "$@"
endif
ifeq ($(MODE),embedded)
$(BUILD_DIR)/generated:
ifeq ($(OS),Windows_NT)
	@if not exist "$(subst /,\,$@)" mkdir "$(subst /,\,$@)"
else
	mkdir -p "$@"
endif
$(BUILD_DIR)/bin2h$(EXEEXT): source/MISL/tools/bin2h.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c11 "$<" -o "$@"
$(BUILD_DIR)/generated/game_roap_bin.h: $(ROAP_FILE) $(BUILD_DIR)/bin2h$(EXEEXT) | $(BUILD_DIR)/generated
	"$(BUILD_DIR)/bin2h$(EXEEXT)" "$(ROAP_FILE)" "$@" game_roap_code
$(BUILD_DIR)/source/MISL/host/runtime/misl_embedded.o: $(BUILD_DIR)/generated/game_roap_bin.h
endif
-include $(OBJECTS:.o=.d) $(PROJECT_OBJECTS:.o=.d)
