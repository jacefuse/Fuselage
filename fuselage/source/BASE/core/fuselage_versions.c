#include "fuselage_versions.h"
#include "../fuselage_configuration.h"
#include "../../GDMF/gdmf.h"
#include "../../GDMF/vulkan/gdmf_vulkan.h"
#include "../../DICE/dice.h"
#include "../../CAKE/cake.h"
#include "../../SHARP/sharp.h"
#include "../../SRAM/sram.h"
#include "../../MISL/host/misl_host.h"
#include "../../MISL/misl_abi.h"
const FuselageComponentVersion* fuselage_get_component_versions(size_t* count) {
    static const FuselageComponentVersion versions[] = {{"Fuselage", FUSELAGE_VERSION},
                                                        {"Text Layer", GDMF_TEXTLAYER_VERSION},
                                                        {"GDMF", GDMF_VERSION},
                                                        {"Sprites", GDMF_SPRITES_VERSION},
                                                        {"Tiles", GDMF_TILES_VERSION},
                                                        {"Pixies", GDMF_PIXIES_VERSION},
                                                        {"Colors", GDMF_COLORS_VERSION},
                                                        {"Interactions", GDMF_INTERACTIONS_VERSION},
                                                        {"Vulkan", GDMF_VULKAN_VERSION},
                                                        {"DICE", DICE_VERSION},
                                                        {"CAKE", CAKE_VERSION},
                                                        {"SHARP", SHARP_VERSION},
                                                        {"SRAM", SRAM_VERSION},
                                                        {"VIVIAN", NULL},
                                                        {"MISL host", FUSELAGE_GUEST_VERSION},
                                                        {"MISL", MISL_VERSION}};
    if (count) {
        *count = sizeof versions / sizeof *versions;
    }
    return versions;
}
