# Makefile for Caketest
#
# Caketest is a combined test of the CAKE input system (keyboard, mouse,
# and controllers) for Fuselage. It can be built without the need of
# any other Fuselage component.

# Platform detection
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

# Platform-specific settings
ifeq ($(PLATFORM),Windows)
    CC      = clang
    TARGET  = Caketest.exe
    LDFLAGS = -luser32 -lhid -lsetupapi -lxinput
else ifeq ($(PLATFORM),Darwin)
    CC      = cc
    TARGET  = caketest
    LDFLAGS = -framework IOKit -framework CoreFoundation
else
    CC      = cc
    TARGET  = caketest
    LDFLAGS =
endif

# Common flags
CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic
SRCS    = cake.c caketest.c
OBJS    = $(SRCS:.c=.o)

# Default target
.PHONY: all debug release clean

all: debug

# Debug
debug: CFLAGS += -g -O0 -DDEBUG
debug: $(TARGET)

# Release
release: CFLAGS += -O2 -DNDEBUG
release: $(TARGET)

# Link
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Compile
%.o: %.c cake.h
	$(CC) $(CFLAGS) -c $< -o $@

# Clean
clean:
	rm -f $(OBJS) $(TARGET)
