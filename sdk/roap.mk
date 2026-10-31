# Standalone guest rules. SEE: (BUILDING_A_ROAP.md, Make interface)
.DEFAULT_GOAL := all
.DELETE_ON_ERROR:
ifndef SDK_DIR
SDK_DIR := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))
endif
MISL_DIR ?= $(SDK_DIR)/../fuselage/source/MISL
RISCV ?= riscv32-unknown-elf-
GUEST_CC ?= $(RISCV)gcc
GUEST_TARGET_FLAGS ?=
GUEST_MARCH ?= rv32imfd
GUEST_MABI ?= ilp32d
GUEST_CFLAGS ?= -Os -Wall -Wextra
GUEST_CPPFLAGS ?=
GUEST_LDFLAGS ?=
GUEST_LIBS ?=
GUEST_COMPILER_INCLUDE ?= $(shell "$(GUEST_CC)" $(GUEST_TARGET_FLAGS) -print-file-name=include)
SOURCES ?= misl.c $(EXTRA_SRC)
BUILD_DIR ?= build
LDSCRIPT ?= $(MISL_DIR)/linker/roap.ld
ifeq ($(OS),Windows_NT)
SHELL := cmd.exe
HOSTEXE := .exe
else
HOSTEXE :=
endif
ROAPPACK ?= $(SDK_DIR)/../fuselage/build/tools/roappack$(HOSTEXE)

ifeq ($(strip $(NAME)),)
$(error NAME must name the application before including roap.mk)
endif
ifneq ($(words $(NAME)),1)
$(error NAME must not contain whitespace)
endif
ifneq ($(strip $(findstring /,$(NAME))$(findstring \,$(NAME))$(findstring :,$(NAME))),)
$(error NAME must be a filename without directory components)
endif
ifeq ($(strip $(BUILD_DIR)),)
$(error BUILD_DIR must not be empty)
endif
ifneq ($(words $(BUILD_DIR) $(SDK_DIR) $(MISL_DIR) $(LDSCRIPT) $(ROAPPACK)),5)
$(error Build paths must be nonempty and contain no whitespace; use relative paths from a spaced checkout)
endif
ifneq ($(words $(SOURCES)),$(words $(sort $(basename $(notdir $(SOURCES))))))
$(error Source basenames must be unique)
endif
ifneq ($(strip $(filter-out %.c %.S %.s,$(SOURCES))),)
$(error SOURCES supports C and assembly files only)
endif

ROAP := $(BUILD_DIR)/$(NAME).roap
ELF := $(BUILD_DIR)/$(NAME).elf
FULL := $(BUILD_DIR)/$(NAME)_full.roap
roap_runtime := guest/runtime/misl_guest_cache.c guest/runtime/misl_guest_heap.c guest/runtime/misl_rt.S format/misl_format.c format/misl_format_numbers.c
roap_app_objects := $(addprefix $(BUILD_DIR)/app/,$(addsuffix .o,$(basename $(notdir $(SOURCES)))))
roap_runtime_objects := $(addprefix $(BUILD_DIR)/runtime/,$(addsuffix .o,$(basename $(notdir $(roap_runtime)))))
roap_objects := $(roap_app_objects) $(roap_runtime_objects)
roap_dependencies := $(roap_objects:.o=.d)
roap_options := $(BUILD_DIR)/options.txt
roap_configuration = CC=$(GUEST_CC)|TARGET=$(GUEST_TARGET_FLAGS)|ARCH=$(GUEST_MARCH)|ABI=$(GUEST_MABI)|CFLAGS=$(GUEST_CFLAGS)|CPPFLAGS=$(GUEST_CPPFLAGS)|EXTRA_INC=$(EXTRA_INC)|LDFLAGS=$(GUEST_LDFLAGS)|LIBS=$(GUEST_LIBS)|INCLUDE=$(GUEST_COMPILER_INCLUDE)|MISL=$(MISL_DIR)|LD=$(LDSCRIPT)|SOURCES=$(SOURCES)|ASSETS=$(ASSETS)|PACKER=$(ROAPPACK)|SECTIONS=$(ROM_SECTIONS)
roap_compile = "$(GUEST_CC)" $(GUEST_TARGET_FLAGS) -march=$(GUEST_MARCH) -mabi=$(GUEST_MABI) $(GUEST_CFLAGS) $(GUEST_CPPFLAGS) -ffreestanding -fno-pic -fno-pie -fno-stack-protector -msmall-data-limit=0 -mno-relax -ffunction-sections -fno-fast-math -ffp-contract=off -fno-math-errno -nostdinc -isystem "$(GUEST_COMPILER_INCLUDE)" -I"$(MISL_DIR)/include" -I"$(MISL_DIR)" -I. $(EXTRA_INC)

# A changed configuration must rebuild even on Make hosts with coarse timestamps.
ifneq ($(roap_configuration),$(file <$(roap_options)))
.PHONY: $(roap_objects) $(ELF) $(ROAP) $(FULL)
endif

.PHONY: all clean info roap-force
all: $(ROAP)
$(BUILD_DIR) $(BUILD_DIR)/app $(BUILD_DIR)/runtime:
ifeq ($(OS),Windows_NT)
	@if not exist "$(subst /,\,$@)" mkdir "$(subst /,\,$@)"
else
	mkdir -p "$@"
endif

# GNU Make compares content itself; parsing the file never runs a write command.
$(roap_options): roap-force | $(BUILD_DIR)
	$(if $(and $(findstring $(roap_configuration),$(file <$(roap_options))),$(findstring $(file <$(roap_options)),$(roap_configuration))),,$(file >$(roap_options),$(roap_configuration)))

define roap_source_rule
$(BUILD_DIR)/app/$(basename $(notdir $(1))).o: $(1) $(MAKEFILE_LIST) $(ASSETS) $(roap_options) | $(BUILD_DIR)/app
	$$(roap_compile) -MMD -MP -MF "$$(@:.o=.d)" -MT "$$@" -c "$$<" -o "$$@"
endef
$(foreach source,$(SOURCES),$(eval $(call roap_source_rule,$(source))))

define roap_runtime_rule
$(BUILD_DIR)/runtime/$(basename $(notdir $(1))).o: $(MISL_DIR)/$(1) $(MAKEFILE_LIST) $(roap_options) | $(BUILD_DIR)/runtime
	$$(roap_compile) -MMD -MP -MF "$$(@:.o=.d)" -MT "$$@" -c "$$<" -o "$$@"
endef
$(foreach source,$(roap_runtime),$(eval $(call roap_runtime_rule,$(source))))

$(ELF): $(roap_objects) $(LDSCRIPT) $(MAKEFILE_LIST) $(roap_options)
	"$(GUEST_CC)" $(GUEST_TARGET_FLAGS) -march=$(GUEST_MARCH) -mabi=$(GUEST_MABI) -nostdlib -nostartfiles -Wl,--no-relax -Wl,--gc-sections -Wl,--build-id=none -Wl,--no-warn-rwx-segments -Wl,-T,"$(LDSCRIPT)" $(GUEST_LDFLAGS) $(foreach object,$(roap_objects),"$(object)") $(GUEST_LIBS) -o "$@"
# An explicit rule, so Make never reaches for its built-in `%: %.o` when roappack.o is newer than roappack. Fails only when the packer is missing.
$(ROAPPACK):
ifeq ($(OS),Windows_NT)
	@if not exist "$(subst /,\,$@)" (echo roappack not found at $@. Build it with make -C fuselage/source/ROAP/tools in the engine tree, or set ROAPPACK. 1>&2 & exit 1)
else
	@test -x "$@" || { echo "roappack not found at $@. Build it with make -C fuselage/source/ROAP/tools in the engine tree, or set ROAPPACK." 1>&2; exit 1; }
endif
$(ROAP): $(ELF) $(ROAPPACK)
	"$(ROAPPACK)" --flatten "$<" -o "$@"

ifneq ($(strip $(ROM_SECTIONS)),)
$(foreach section,$(ROM_SECTIONS),$(if $(filter 2,$(words $(subst =, ,$(section)))),,$(error ROM_SECTIONS entries must be id=path without spaces or extra equals signs)))
roap_asset_files := $(foreach section,$(ROM_SECTIONS),$(word 2,$(subst =, ,$(section))))
$(FULL): $(ROAP) $(ROAPPACK) $(roap_asset_files) $(roap_options)
	"$(ROAPPACK)" -o "$@" --image "$(ROAP)" $(foreach section,$(ROM_SECTIONS),"$(section)")
all: $(FULL)
endif

info:
	@echo "name: $(NAME)"
	@echo "compiler: $(GUEST_CC) $(GUEST_TARGET_FLAGS)"
	@echo "guest: $(GUEST_MARCH)/$(GUEST_MABI)"
	@echo "output: $(ROAP)"
clean:
ifeq ($(OS),Windows_NT)
	@for %%f in ($(foreach output,$(roap_objects) $(roap_dependencies) $(ELF) $(ROAP) $(FULL) $(roap_options),"$(subst /,\,$(output))")) do @if exist "%%~f" del /q "%%~f"
else
	rm -f $(foreach output,$(roap_objects) $(roap_dependencies) $(ELF) $(ROAP) $(FULL) $(roap_options),"$(output)")
endif
-include $(roap_dependencies)
