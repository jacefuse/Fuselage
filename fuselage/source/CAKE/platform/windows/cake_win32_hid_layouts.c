// Controller layouts and HID axis usages. SEE: (CAKE.md, Windows controllers)
#include "cake_win32_controller_private.h"
#if defined(_WIN32)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static const cake_win32_hid_layout cake_win32_hid_layouts[] = {

    {"generic",
     {0, CAKE_BUTTON_A, CAKE_BUTTON_B, 0, CAKE_BUTTON_X, CAKE_BUTTON_Y, 0, CAKE_BUTTON_LEFT_SHOULDER,
      CAKE_BUTTON_RIGHT_SHOULDER, 0, 0, CAKE_BUTTON_BACK, CAKE_BUTTON_START, 0, CAKE_BUTTON_LEFT_THUMB,
      CAKE_BUTTON_RIGHT_THUMB, 0},
     9,
     10},

    {"sony",
     {0, CAKE_BUTTON_Y, CAKE_BUTTON_A, CAKE_BUTTON_B, CAKE_BUTTON_X, CAKE_BUTTON_LEFT_SHOULDER,
      CAKE_BUTTON_RIGHT_SHOULDER, 0, 0, CAKE_BUTTON_BACK, CAKE_BUTTON_START, CAKE_BUTTON_LEFT_THUMB,
      CAKE_BUTTON_RIGHT_THUMB, 0, 0, 0, 0},
     7,
     8},

    {"nintendo",
     {0, CAKE_BUTTON_A, CAKE_BUTTON_B, CAKE_BUTTON_Y, CAKE_BUTTON_X, CAKE_BUTTON_LEFT_SHOULDER,
      CAKE_BUTTON_RIGHT_SHOULDER, 0, 0, CAKE_BUTTON_BACK, CAKE_BUTTON_START, CAKE_BUTTON_LEFT_THUMB,
      CAKE_BUTTON_RIGHT_THUMB, 0, 0, 0, 0},
     7,
     8},

    {"neogeox",
     {0, CAKE_BUTTON_A, CAKE_BUTTON_B, CAKE_BUTTON_X, CAKE_BUTTON_Y, 0, 0, 0, 0, CAKE_BUTTON_BACK,
      CAKE_BUTTON_START, 0, 0, 0, 0, 0, 0},
     0,
     0},
};

const cake_win32_hid_layout* cake_win32_hid_layout_for(uint16_t vendor_id, uint16_t product_id) {
    if (vendor_id == 0x1292 && product_id == 0x4E47) {
        return &cake_win32_hid_layouts[3];
    }

    switch (vendor_id) {
    case 0x054C:
        return &cake_win32_hid_layouts[1];
    case 0x057E:
        return &cake_win32_hid_layouts[2];
    default:
        return &cake_win32_hid_layouts[0];
    }
}

const cake_win32_hid_usage cake_win32_hid_axis_usage[CAKE_WIN32_HID_AXIS_COUNT] = {
    {0x01, 0x30}, {0x01, 0x31}, {0x01, 0x32}, {0x01, 0x33}, {0x01, 0x34},
    {0x01, 0x35}, {0x01, 0x39}, {0x02, 0xC5}, {0x02, 0xC4},
};

const char* const cake_win32_hid_axis_name[CAKE_WIN32_HID_AXIS_COUNT] = {"X",  "Y",   "Z",     "Rx",   "Ry",
                                                                         "Rz", "Hat", "Brake", "Accel"};

#endif
