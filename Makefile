# Makefile for Fuselage BUTTOCKS
# debug: console subsystem -- printf log visible in terminal (good for development)
# release: Windows subsystem (-mwindows) -- no console window spawned

UNAME_S := $(shell uname -s)

ifneq (,$(findstring MINGW,$(UNAME_S)))
    PLATFORM = Windows
else ifneq (,$(findstring MSYS,$(UNAME_S)))
    PLATFORM = Windows
else ifeq ($(UNAME_S),Darwin)
    PLATFORM = Darwin
else
    PLATFORM = Linux
endif

ifeq ($(PLATFORM),Windows)
    CC          = clang
    TARGET      = fuselage.exe
    LDFLAGS     = -luser32 -lgdi32 -lhid -lsetupapi -lxinput -lm
    SURFACE_SRC = GDMF/gdmf_surface_win32.c
    SPV2H       = $(SHADER_DIR)/spv2h.exe
else ifeq ($(PLATFORM),Darwin)
    CC      = cc
    TARGET  = fuselage
    LDFLAGS = -framework IOKit -framework CoreFoundation -lm
    SPV2H   = $(SHADER_DIR)/spv2h
    # SURFACE_SRC: no macOS surface file yet -- add GDMF/gdmf_surface_macos.c
    # here when that port is actually undertaken.
else
    CC      = cc
    TARGET  = fuselage
    LDFLAGS = -lm
    SPV2H   = $(SHADER_DIR)/spv2h
    # SURFACE_SRC: no Linux surface file yet -- add GDMF/gdmf_surface_*.c
    # (xcb/wayland) here when that port is actually undertaken.
endif

CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -I. -DFUSELAGE_HAS_GDMF

# Vulkan: use VULKAN_SDK if set (official SDK), otherwise assume MSYS2 packages
ifdef VULKAN_SDK
    CFLAGS  += -I"$(VULKAN_SDK)/Include"
    LDFLAGS += -L"$(VULKAN_SDK)/Lib" -lvulkan-1
    GLSLC   = "$(VULKAN_SDK)/Bin/glslc"
else
    LDFLAGS += -lvulkan-1
    GLSLC   = glslc
endif

SRCS = main.c \
       fuselage.c \
       GDMF/gdmf.c \
       GDMF/gdmf_vulkan.c \
       $(SURFACE_SRC) \
       GDMF/gdmf_textlayer.c \
       GDMF/colors.c \
       GDMF/gdmf_sprites.c \
       GDMF/gdmf_tiles.c \
       GDMF/gdmf_pixies.c \
       CAKE/cake.c \
       CAKE/cake_help.c

OBJS = $(SRCS:.c=.o)

SHADER_DIR = GDMF/shaders

.PHONY: all debug release clean

all: debug

debug: CFLAGS += -g -O0 -DDEBUG
debug: $(TARGET)

release: CFLAGS  += -O2 -DNDEBUG
release: LDFLAGS += -mwindows
release: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Shader pipeline: GLSL -> SPIR-V -> embedded C header, so editing a
# .vert/.frag and rebuilding can never silently run against a stale compiled
# shader. spv2h is built from source on first use -- no external tool needed.
$(SPV2H): $(SHADER_DIR)/spv2h.c
	$(CC) -o $@ $<

$(SHADER_DIR)/%_vert.spv: $(SHADER_DIR)/%.vert
	$(GLSLC) -fshader-stage=vertex -o $@ $<

$(SHADER_DIR)/%_frag.spv: $(SHADER_DIR)/%.frag
	$(GLSLC) -fshader-stage=fragment -o $@ $<

$(SHADER_DIR)/%.h: $(SHADER_DIR)/%.spv $(SPV2H)
	$(SPV2H) $< $@

# Explicit shader-header prerequisites for the .c files that #include them
# -- the only thing make couldn't infer on its own here is *which* object
# files depend on *which* shader headers, since that's expressed inside the
# .c files' #include lines rather than anywhere make can see directly.
GDMF/gdmf_sprites.o: $(SHADER_DIR)/sprite_vert.h $(SHADER_DIR)/sprite_frag.h
GDMF/gdmf_textlayer.o: $(SHADER_DIR)/text_vert.h $(SHADER_DIR)/text_frag.h
GDMF/gdmf_tiles.o: $(SHADER_DIR)/tile_vert.h $(SHADER_DIR)/tile_frag.h
GDMF/gdmf_pixies.o: $(SHADER_DIR)/pixie_vert.h $(SHADER_DIR)/pixie_frag.h $(SHADER_DIR)/pixie_live_vert.h $(SHADER_DIR)/pixie_live_frag.h

# Without this, make treats .spv as a disposable intermediate in the
# .vert/.frag -> .spv -> .h chain (nothing else depends on it directly) and
# deletes it once the .h is built. Keeping it on disk matches the
# pre-existing repo layout and leaves the raw SPIR-V available for
# inspection (spirv-dis, etc.) without needing to recompile it by hand.
.SECONDARY:

clean:
	rm -f $(OBJS) $(TARGET) $(SPV2H)
