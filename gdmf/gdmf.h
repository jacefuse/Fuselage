#pragma once
// ---- Graphic Device Minimalist Framework ----
// GDMF is intended as a minimalist interface between Fuselage and Vulkan.
// It is not intended as an entire software development framework.
// While it may be possible to use it beyond it's intended purpose of
// providing a foundation for the Fuselage Virtual Machine it is
// recommended that for general purpose use a more suitable framework
// should be selected. GLFW, RGFW, RayLib, or SDL might be a better choice.

// Fuselage is provided as free software in an opened source nature.
// It should be considered experimental. No retrictions are in place.
// With no restrictions comes no warranty or liability.
// Any possible support is only offered on a purely voluntary basis.

#include <windows.h>
#include <tchar.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vulkan.h>
#include <vulkan_win32.h>

#include "colors.h"
#include "gdmf_internal.h"
#include "gdmf_vulkan_device.h"
#include "gdmf_vulkan_command.h"
#include "gdmf_vulkan_sync.h"
#include "gdmf_vulkan_pipeline.h"
#include "gdmf_vulkan_renderloop.h"
#include "gdmf_vulkan_shaders.h"
#include "gdmf_textlayer.h"


#define FUSELAGE_GDMF_VERSION "0.2.25082201 BUTTOCKS"
#define FUSELAGE_APPLICATION_NAME "Fuselage Demonstration Application"

// Main facing functions - Public API
int GDMFinit(void);
void GDMFshutdown(void);

// Frame rendering with synchronization
int GDMFrenderFrame(void);