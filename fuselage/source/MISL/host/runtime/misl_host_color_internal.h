/* Color conversion is a native graphics adapter concern. */
#ifndef MISL_HOST_COLOR_INTERNAL_H
#define MISL_HOST_COLOR_INTERNAL_H
#include "misl_host_arguments_internal.h"
#include "../../../GDMF/gdmf_colors.h"

static inline Color fuselage_guest_get_color_argument(unsigned index) {
    uint32_t packed = fuselage_guest_get_argument(index);
    return (Color){(unsigned char)packed, (unsigned char)(packed >> 8), (unsigned char)(packed >> 16),
                   (unsigned char)(packed >> 24)};
}
static inline void fuselage_guest_set_color_result(Color color) {
    fuselage_guest_set_result(PackColorsRGBA8(color));
}
#endif
