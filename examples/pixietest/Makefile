# Makefile for Fuselage BUTTOCKS example: PixieTest
# Builds main.c against the components living two directories up
# (fuselage.c, GDMF/, CAKE/), without touching that tree.

ROOT = ../..

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
    TARGET      = pixietest.exe
    LDFLAGS     = -luser32 -lgdi32 -lhid -lsetupapi -lxinput -lm
    SURFACE_SRC = $(ROOT)/GDMF/gdmf_surface_win32.c
    SPV2H       = $(SHADER_DIR)/spv2h.exe
else ifeq ($(PLATFORM),Darwin)
    CC      = cc
    TARGET  = pixietest
    LDFLAGS = -framework IOKit -framework CoreFoundation -lm
    SPV2H   = $(SHADER_DIR)/spv2h
else
    CC      = cc
    TARGET  = pixietest
    LDFLAGS = -lm
    SPV2H   = $(SHADER_DIR)/spv2h
endif

CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -I. -I$(ROOT) -DFUSELAGE_HAS_GDMF

ifdef VULKAN_SDK
    CFLAGS  += -I"$(VULKAN_SDK)/Include"
    LDFLAGS += -L"$(VULKAN_SDK)/Lib" -lvulkan-1
    GLSLC   = "$(VULKAN_SDK)/Bin/glslc"
else
    LDFLAGS += -lvulkan-1
    GLSLC   = glslc
endif

SRCS = main.c \
       $(ROOT)/fuselage.c \
       $(ROOT)/GDMF/gdmf.c \
       $(ROOT)/GDMF/gdmf_vulkan.c \
       $(SURFACE_SRC) \
       $(ROOT)/GDMF/gdmf_textlayer.c \
       $(ROOT)/GDMF/colors.c \
       $(ROOT)/GDMF/gdmf_sprites.c \
       $(ROOT)/GDMF/gdmf_tiles.c \
       $(ROOT)/GDMF/gdmf_pixies.c \
       $(ROOT)/CAKE/cake.c \
       $(ROOT)/CAKE/cake_help.c

OBJS = $(SRCS:.c=.o)

SHADER_DIR = $(ROOT)/GDMF/shaders

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

# Shader pipeline is shared with the root build -- reuses whatever spv2h/.h
# files already exist under GDMF/shaders rather than rebuilding them here.
$(SPV2H): $(SHADER_DIR)/spv2h.c
	$(CC) -o $@ $<

$(SHADER_DIR)/%_vert.spv: $(SHADER_DIR)/%.vert
	$(GLSLC) -fshader-stage=vertex -o $@ $<

$(SHADER_DIR)/%_frag.spv: $(SHADER_DIR)/%.frag
	$(GLSLC) -fshader-stage=fragment -o $@ $<

$(SHADER_DIR)/%.h: $(SHADER_DIR)/%.spv $(SPV2H)
	$(SPV2H) $< $@

$(ROOT)/GDMF/gdmf_sprites.o: $(SHADER_DIR)/sprite_vert.h $(SHADER_DIR)/sprite_frag.h
$(ROOT)/GDMF/gdmf_textlayer.o: $(SHADER_DIR)/text_vert.h $(SHADER_DIR)/text_frag.h
$(ROOT)/GDMF/gdmf_tiles.o: $(SHADER_DIR)/tile_vert.h $(SHADER_DIR)/tile_frag.h
$(ROOT)/GDMF/gdmf_pixies.o: $(SHADER_DIR)/pixie_vert.h $(SHADER_DIR)/pixie_frag.h $(SHADER_DIR)/pixie_live_vert.h $(SHADER_DIR)/pixie_live_frag.h

.SECONDARY:

clean:
	rm -f $(OBJS) $(TARGET) $(SPV2H)
