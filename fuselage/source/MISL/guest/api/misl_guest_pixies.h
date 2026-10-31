#pragma once
#include "misl_guest_memory.h"

// Pixie ecalls, coordinate packing and command-list helpers.
// SEE: (MISL_GUEST.md, Pixies and command lists)

static inline bool InitPixie(int id, PixieMode mode, int output_width, int output_height) {
    return MISL_GUEST_CALL(MISL_PIX_INIT, id, mode, output_width, output_height, 0, 0, 0) != 0;
}

static inline bool ReleasePixie(int id) {
    return MISL_GUEST_CALL(MISL_PIX_SHUTDOWN, id, 0, 0, 0, 0, 0, 0) != 0;
}

static inline bool IssuePixieCommand(int id, PixieOpcode opcode, uint16_t flags,
                                     const uint32_t arguments[4]) {
    return MISL_GUEST_CALL(MISL_PIX_COMMAND, id, opcode, flags, misl_guest_address(arguments), 0, 0, 0) != 0;
}

static inline uint32_t PackPixieFloatArg(float value) {
    return (uint32_t)misl_guest_float_bits(value);
}

static inline int32_t misl_guest_pack_pixie_coordinate(float coordinate) {
    if (coordinate != coordinate) {
        return 0;
    }
    if (coordinate <= PIXIE_DRAW_COORD_MIN) {
        return -32768;
    }
    if (coordinate >= PIXIE_DRAW_COORD_MAX) {
        return 32767;
    }
    // Retain guest rounding. SEE: (MISL_GUEST.md, Pixies and command lists)
    return (int32_t)(coordinate * 8.0f + (coordinate < 0.0f ? -0.5f : 0.5f));
}

static inline uint32_t PackPixieDrawXY(float coordinate_x, float coordinate_y) {
    int32_t packed_x = misl_guest_pack_pixie_coordinate(coordinate_x);
    int32_t packed_y = misl_guest_pack_pixie_coordinate(coordinate_y);

    return ((uint32_t)(packed_x & 0xFFFF) << 16) | (uint32_t)(packed_y & 0xFFFF);
}

static inline bool misl_guest_clip_line_parameter(float direction, float distance, float* start_fraction,
                                                  float* end_fraction) {
    if (direction == 0.0f) {
        return distance >= 0.0f;
    }

    float intersection = distance / direction;
    if (direction < 0.0f) {
        if (intersection > *end_fraction) {
            return false;
        }
        if (intersection > *start_fraction) {
            *start_fraction = intersection;
        }
    } else {
        if (intersection < *start_fraction) {
            return false;
        }
        if (intersection < *end_fraction) {
            *end_fraction = intersection;
        }
    }

    return true;
}

static inline bool PackPixieDrawLine(float start_x, float start_y, float end_x, float end_y,
                                     uint32_t packed_endpoints[2]) {
    if (!packed_endpoints || !__builtin_isfinite(start_x) || !__builtin_isfinite(start_y) ||
        !__builtin_isfinite(end_x) || !__builtin_isfinite(end_y)) {
        return false;
    }

    if (start_x >= PIXIE_DRAW_COORD_MIN && start_x <= PIXIE_DRAW_COORD_MAX &&
        start_y >= PIXIE_DRAW_COORD_MIN && start_y <= PIXIE_DRAW_COORD_MAX && end_x >= PIXIE_DRAW_COORD_MIN &&
        end_x <= PIXIE_DRAW_COORD_MAX && end_y >= PIXIE_DRAW_COORD_MIN && end_y <= PIXIE_DRAW_COORD_MAX) {
        packed_endpoints[0] = PackPixieDrawXY(start_x, start_y);
        packed_endpoints[1] = PackPixieDrawXY(end_x, end_y);

        return true;
    }

    float delta_x = end_x - start_x, delta_y = end_y - start_y;
    if (!__builtin_isfinite(delta_x) || !__builtin_isfinite(delta_y)) {
        return false;
    }
    float start_fraction = 0.0f, end_fraction = 1.0f;

    if (!misl_guest_clip_line_parameter(-delta_x, start_x - PIXIE_DRAW_COORD_MIN, &start_fraction,
                                        &end_fraction) ||
        !misl_guest_clip_line_parameter(delta_x, PIXIE_DRAW_COORD_MAX - start_x, &start_fraction,
                                        &end_fraction) ||
        !misl_guest_clip_line_parameter(-delta_y, start_y - PIXIE_DRAW_COORD_MIN, &start_fraction,
                                        &end_fraction) ||
        !misl_guest_clip_line_parameter(delta_y, PIXIE_DRAW_COORD_MAX - start_y, &start_fraction,
                                        &end_fraction)) {
        return false;
    }

    packed_endpoints[0] =
        PackPixieDrawXY(start_x + start_fraction * delta_x, start_y + start_fraction * delta_y);
    packed_endpoints[1] = PackPixieDrawXY(start_x + end_fraction * delta_x, start_y + end_fraction * delta_y);

    return true;
}

static inline bool WritePixieRAM(int id, uint32_t offset, const void* data, uint32_t size) {
    return MISL_GUEST_CALL(MISL_PIX_WRITE, id, offset, misl_guest_address(data), size, 0, 0, 0) != 0;
}

static inline uint32_t GetPixieRAMSize(int id) {
    return (uint32_t)MISL_GUEST_CALL(MISL_PIX_GET_RAM_SIZE, id, 0, 0, 0, 0, 0, 0);
}

static inline uint32_t ReadPixieString(int id, char* buffer, uint32_t capacity) {
    return (uint32_t)MISL_GUEST_CALL(MISL_PIX_READ_STRING, id, misl_guest_address(buffer), capacity, 0, 0, 0,
                                     0);
}

static inline bool SetPixiePosition(int id, float x, float y) {
    return MISL_GUEST_CALL(MISL_PIX_SET_POSITION, id, misl_guest_float_bits(x), misl_guest_float_bits(y), 0,
                           0, 0, 0) != 0;
}

static inline bool SetPixieDisplaySize(int id, float width, float height) {
    return MISL_GUEST_CALL(MISL_PIX_SET_DISPLAY_SIZE, id, misl_guest_float_bits(width),
                           misl_guest_float_bits(height), 0, 0, 0, 0) != 0;
}

static inline bool SetPixiePriority(int id, unsigned char priority) {
    return MISL_GUEST_CALL(MISL_PIX_SET_PRIORITY, id, priority, 0, 0, 0, 0, 0) != 0;
}

static inline bool SetPixieEnabled(int id, bool enabled) {
    return MISL_GUEST_CALL(MISL_PIX_SET_ENABLED, id, enabled, 0, 0, 0, 0, 0) != 0;
}

static inline bool ShowPixie(int id) {
    return MISL_GUEST_CALL(MISL_PIX_SHOW, id, 0, 0, 0, 0, 0, 0) != 0;
}

static inline bool HidePixie(int id) {
    return MISL_GUEST_CALL(MISL_PIX_HIDE, id, 0, 0, 0, 0, 0, 0) != 0;
}

static inline bool SetPixieDrawPattern(int id, uint16_t pattern) {
    return MISL_GUEST_CALL(MISL_PIX_SET_DRAW_PATTERN, id, pattern, 0, 0, 0, 0, 0) != 0;
}

static inline bool SetPixiePatternBitmap(int id, uint32_t ram_offset, uint16_t width, uint16_t height) {
    return MISL_GUEST_CALL(MISL_PIX_SET_PATTERN_BITMAP, id, ram_offset, width, height, 0, 0, 0) != 0;
}

static inline bool ClearPixiePatternBitmap(int id) {
    return MISL_GUEST_CALL(MISL_PIX_CLEAR_PATTERN_BITMAP, id, 0, 0, 0, 0, 0, 0) != 0;
}

static inline uint16_t GetPixieDrawPattern(int id) {
    return (uint16_t)MISL_GUEST_CALL(MISL_PIX_GET_DRAW_PATTERN, id, 0, 0, 0, 0, 0, 0);
}

static inline bool GetPixieInitialized(int id) {
    return MISL_GUEST_CALL(MISL_PIX_GET_INITIALIZED, id, 0, 0, 0, 0, 0, 0) != 0;
}

static inline PixieMode GetPixieMode(int id) {
    return (PixieMode)MISL_GUEST_CALL(MISL_PIX_GET_MODE, id, 0, 0, 0, 0, 0, 0);
}

static inline int GetPixieOutputWidth(int id) {
    return MISL_GUEST_CALL(MISL_PIX_GET_OUT_WIDTH, id, 0, 0, 0, 0, 0, 0);
}

static inline int GetPixieOutputHeight(int id) {
    return MISL_GUEST_CALL(MISL_PIX_GET_OUT_HEIGHT, id, 0, 0, 0, 0, 0, 0);
}

static inline float GetPixieX(int id) {
    return misl_guest_bits_float(MISL_GUEST_CALL(MISL_PIX_GET_X, id, 0, 0, 0, 0, 0, 0));
}

static inline float GetPixieY(int id) {
    return misl_guest_bits_float(MISL_GUEST_CALL(MISL_PIX_GET_Y, id, 0, 0, 0, 0, 0, 0));
}

static inline float GetPixieDisplayWidth(int id) {
    return misl_guest_bits_float(MISL_GUEST_CALL(MISL_PIX_GET_DISPLAY_WIDTH, id, 0, 0, 0, 0, 0, 0));
}

static inline float GetPixieDisplayHeight(int id) {
    return misl_guest_bits_float(MISL_GUEST_CALL(MISL_PIX_GET_DISPLAY_HEIGHT, id, 0, 0, 0, 0, 0, 0));
}

static inline unsigned char GetPixiePriority(int id) {
    return (unsigned char)MISL_GUEST_CALL(MISL_PIX_GET_PRIORITY, id, 0, 0, 0, 0, 0, 0);
}

static inline bool GetPixieEnabled(int id) {
    return MISL_GUEST_CALL(MISL_PIX_GET_ENABLED, id, 0, 0, 0, 0, 0, 0) != 0;
}

static inline bool GetPixieShown(int id) {
    return MISL_GUEST_CALL(MISL_PIX_GET_SHOWN, id, 0, 0, 0, 0, 0, 0) != 0;
}

static inline volatile MislPixieCmd* GetPixieList(void) {
    return (volatile MislPixieCmd*)(uintptr_t)misl_guest_region_bases[MISL_REGION_PIXIE_LIST];
}

static inline bool IsPixieListAvailable(void) {
    return misl_guest_region_bases[MISL_REGION_PIXIE_LIST] != 0u;
}

static inline uint32_t GetPixieListCapacity(void) {
    return MISL_PIXIE_LIST_MAX;
}

static inline uint32_t PushPixieListCommand(uint32_t command_index, int pixie, PixieOpcode opcode,
                                            uint16_t flags, const uint32_t args[4]) {
    volatile MislPixieCmd* list = GetPixieList();
    if (list && command_index < MISL_PIXIE_LIST_MAX) {
        list[command_index].pixie = (uint8_t)pixie;
        list[command_index].opcode = (uint8_t)opcode;
        list[command_index].flags = flags;
        list[command_index].args[0] = args[0];
        list[command_index].args[1] = args[1];
        list[command_index].args[2] = args[2];
        list[command_index].args[3] = args[3];
    }
    return command_index + 1u;
}

static inline uint32_t SubmitPixieList(uint32_t count) {
    return (uint32_t)MISL_GUEST_CALL(MISL_PIX_SUBMIT, count, 0, 0, 0, 0, 0, 0);
}

static inline bool ExecutePixieListAt(int id, uint32_t ram_offset, uint32_t count, int slot) {
    uint32_t arguments[4];
    arguments[0] = ram_offset;
    arguments[1] = count;
    arguments[2] = (uint32_t)slot;
    arguments[3] = 0;
    return IssuePixieCommand(id, PIXIE_OP_EXECUTE, 0, arguments);
}

static inline bool ExecutePixieList(int id, uint32_t ram_offset, uint32_t count) {
    return ExecutePixieListAt(id, ram_offset, count, 0);
}

static inline bool SetPixieTransform(int id, int slot, float translation_x, float translation_y,
                                     float scale_x, float scale_y, float rotation_degrees) {
    return MISL_GUEST_CALL(MISL_PIX_SET_TRANSFORM, id, slot, misl_guest_float_bits(translation_x),
                           misl_guest_float_bits(translation_y), misl_guest_float_bits(scale_x),
                           misl_guest_float_bits(scale_y), misl_guest_float_bits(rotation_degrees)) != 0;
}

static inline bool SetPixieTransformMatrix(int id, int slot, const PixieTransform* matrix) {
    return MISL_GUEST_CALL(MISL_PIX_SET_TRANSFORM_M, id, slot, misl_guest_address(matrix), 0, 0, 0, 0) != 0;
}

static inline bool GetPixieTransformMatrix(int id, int slot, PixieTransform* output) {
    return MISL_GUEST_CALL(MISL_PIX_GET_TRANSFORM_M, id, slot, misl_guest_address(output), 0, 0, 0, 0) != 0;
}

static inline bool ResetPixieTransform(int id, int slot) {
    return MISL_GUEST_CALL(MISL_PIX_RESET_TRANSFORM, id, slot, 0, 0, 0, 0, 0) != 0;
}

static inline bool SetPixieCurrentTransform(int id, int slot) {
    return MISL_GUEST_CALL(MISL_PIX_SET_CUR_TRANSFORM, id, slot, 0, 0, 0, 0, 0) != 0;
}

static inline bool SetPixieMaskInvert(int id, bool invert) {
    return MISL_GUEST_CALL(MISL_PIX_SET_MASK_INVERT, id, invert, 0, 0, 0, 0, 0) != 0;
}

static inline bool GetPixieMaskInvert(int id) {
    return MISL_GUEST_CALL(MISL_PIX_GET_MASK_INVERT, id, 0, 0, 0, 0, 0, 0) != 0;
}

static inline bool SetPixieRotation(int id, float radians) {
    return MISL_GUEST_CALL(MISL_PIX_SET_ROTATION, id, misl_guest_float_bits(radians), 0, 0, 0, 0, 0) != 0;
}

static inline float GetPixieRotation(int id) {
    return misl_guest_bits_float(MISL_GUEST_CALL(MISL_PIX_GET_ROTATION, id, 0, 0, 0, 0, 0, 0));
}

static inline bool SetPixieHotspot(int id, float hotspot_x, float hotspot_y) {
    return MISL_GUEST_CALL(MISL_PIX_SET_HOTSPOT, id, misl_guest_float_bits(hotspot_x),
                           misl_guest_float_bits(hotspot_y), 0, 0, 0, 0) != 0;
}

static inline float GetPixieHotspotX(int id) {
    return misl_guest_bits_float(MISL_GUEST_CALL(MISL_PIX_GET_HOTSPOT_X, id, 0, 0, 0, 0, 0, 0));
}

static inline float GetPixieHotspotY(int id) {
    return misl_guest_bits_float(MISL_GUEST_CALL(MISL_PIX_GET_HOTSPOT_Y, id, 0, 0, 0, 0, 0, 0));
}

static inline bool SetPixieRotateAroundHotspot(int id, bool enabled) {
    return MISL_GUEST_CALL(MISL_PIX_SET_ROT_HOTSPOT, id, enabled, 0, 0, 0, 0, 0) != 0;
}

static inline bool GetPixieRotateAroundHotspot(int id) {
    return MISL_GUEST_CALL(MISL_PIX_GET_ROT_HOTSPOT, id, 0, 0, 0, 0, 0, 0) != 0;
}

static inline bool SetPixieFilterLinear(int id, bool enabled) {
    return MISL_GUEST_CALL(MISL_PIX_SET_FILTER_LINEAR, id, enabled, 0, 0, 0, 0, 0) != 0;
}

static inline bool GetPixieFilterLinear(int id) {
    return MISL_GUEST_CALL(MISL_PIX_GET_FILTER_LINEAR, id, 0, 0, 0, 0, 0, 0) != 0;
}

static inline const volatile void* GetPixieRAMReadPtr(int id) {
    uint32_t base = misl_guest_region_bases[MISL_REGION_PIXIE_RAM];
    if (!base || id < 0 || id >= (int)MISL_PIXIE_RAM_BANKS) {
        return 0;
    }
    return (const volatile void*)(uintptr_t)(base + (uint32_t)id * MISL_PIXIE_RAM_STRIDE);
}

static inline bool DrawPixieLines(int id, uint32_t ram_offset, uint32_t count, uint32_t color, uint32_t width,
                                  int line_mode, uint32_t point_format) {
    uint32_t arguments[4];
    arguments[0] = ram_offset;
    arguments[1] = count;
    arguments[2] = color;
    arguments[3] = width;
    return IssuePixieCommand(id, PIXIE_OP_DRAW_LINES,
                             (uint16_t)(((uint32_t)line_mode & PIXIE_LINES_MODE_MASK) | point_format),
                             arguments);
}

static inline uint32_t PushPixieListLines(uint32_t command_index, int pixie, uint32_t ram_offset,
                                          uint32_t count, uint32_t color, uint32_t width, int line_mode,
                                          uint32_t point_format) {
    uint32_t arguments[4];
    arguments[0] = ram_offset;
    arguments[1] = count;
    arguments[2] = color;
    arguments[3] = width;
    return PushPixieListCommand(command_index, pixie, PIXIE_OP_DRAW_LINES,
                                (uint16_t)(((uint32_t)line_mode & PIXIE_LINES_MODE_MASK) | point_format),
                                arguments);
}

static inline uint32_t PushPixieListTransform(uint32_t command_index, int pixie, int slot) {
    uint32_t arguments[4];
    arguments[0] = (uint32_t)slot;
    arguments[1] = 0;
    arguments[2] = 0;
    arguments[3] = 0;
    return PushPixieListCommand(command_index, pixie, PIXIE_OP_SET_TRANSFORM, 0, arguments);
}

static inline const volatile MislStatus* MISL_GetStatus(void) {
    return (const volatile MislStatus*)(uintptr_t)misl_guest_region_bases[MISL_REGION_STATUS];
}

static inline uint32_t GetPixieListRejectedCount(void) {
    const volatile MislStatus* status = MISL_GetStatus();
    return status ? status->pixieList.rejected : 0u;
}

static inline uint32_t GetPixieListFirstReject(void) {
    const volatile MislStatus* status = MISL_GetStatus();
    return status ? status->pixieList.firstRejectIndex : MISL_PIXIE_NO_REJECT;
}

static inline uint32_t GetPixieListSubmitCount(void) {
    const volatile MislStatus* status = MISL_GetStatus();
    return status ? status->pixieList.submits : 0u;
}

static inline uint32_t GetPixieListRecordCount(void) {
    const volatile MislStatus* status = MISL_GetStatus();
    return status ? status->pixieList.totalExecuted : 0u;
}
