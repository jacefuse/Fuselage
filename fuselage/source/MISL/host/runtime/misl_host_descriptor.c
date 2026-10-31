/* Publish only the region layout that has actually been mapped. */
#include "misl_host_state_internal.h"
#include "../../misl_abi.h"
#include <string.h>

_Static_assert(14u + ROAP_MANIFEST_MAX_POOLS + 2u <= MISL_DESC_MAX_REGIONS,
               "The host region set exceeds the descriptor");

static int fuselage_guest_add_region(MislDescriptor* descriptor, uint32_t id, uint32_t base,
                                     uint32_t byte_count, uint32_t flags) {
    if (descriptor->regionCount >= MISL_DESC_MAX_REGIONS) {
        return 0;
    }
    descriptor->region[descriptor->regionCount++] = (MislRegion){id, base, byte_count, flags};
    return 1;
}

int fuselage_guest_publish_descriptor(void) {
    const MislHostImagePlan* plan = &fuselage_guest_session.plan;
    if (!fuselage_guest_session.memory || fuselage_guest_session.cpu.mem != fuselage_guest_session.memory ||
        plan->pool_count > ROAP_MANIFEST_MAX_POOLS) {
        return 0;
    }
    MislDescriptor descriptor = {0};
    descriptor.magic = MISL_DESC_MAGIC;
    descriptor.abiVersion = MISL_ABI_VERSION;
    descriptor.descBytes = sizeof descriptor;
    int valid = 1;
#define MISL_HOST_REGION(id, base, size, flags)                                                              \
    (valid &= fuselage_guest_add_region(&descriptor, id, base, size, flags))
    MISL_HOST_REGION(MISL_REGION_PROGRAM, 0, plan->program_bytes,
                     MISL_REGION_READ | MISL_REGION_WRITE | MISL_REGION_EXEC);
    for (uint32_t index = 0; index < plan->pool_count; index++) {
        MISL_HOST_REGION(MISL_REGION_POOL + index, plan->pool_base[index], plan->pool_bytes[index],
                         MISL_REGION_READ | MISL_REGION_WRITE);
    }
    MISL_HOST_REGION(MISL_REGION_INPUT, MISL_INPUT_ADDR, sizeof(MislInput), MISL_REGION_READ);
    MISL_HOST_REGION(MISL_REGION_CLOCK, MISL_CLOCK_ADDR, sizeof(MislClock), MISL_REGION_READ);
    MISL_HOST_REGION(MISL_REGION_BOOTSPUR, MISL_ROM_ADDR,
                     fuselage_guest_session.cpu.rom[MISL_HOST_BOOTSPUR_SPAN].size,
                     MISL_REGION_READ | MISL_REGION_EXEC);
    if (fuselage_guest_session.save_byte_count) {
        MISL_HOST_REGION(MISL_REGION_SRAM, plan->save_base, fuselage_guest_session.save_byte_count,
                         MISL_REGION_READ);
    }
    MISL_HOST_REGION(MISL_REGION_DESC, MISL_DESC_ADDR, sizeof descriptor, MISL_REGION_READ);
    MISL_HOST_REGION(MISL_REGION_STATUS, MISL_STATUS_ADDR, MISL_STATUS_BYTES, MISL_REGION_READ);
    MISL_HOST_REGION(MISL_REGION_PIXIE_LIST, MISL_PIXIE_LIST_ADDR, MISL_PIXIE_LIST_BYTES,
                     MISL_REGION_READ | MISL_REGION_WRITE);
    MISL_HOST_REGION(MISL_REGION_PIXIE_RAM, MISL_PIXIE_RAM_ADDR, MISL_PIXIE_RAM_BYTES, MISL_REGION_READ);
    if (plan->asset_byte_count) {
        MISL_HOST_REGION(MISL_REGION_ASSET_ROM, plan->asset_base, plan->asset_byte_count, MISL_REGION_READ);
    }
    MISL_HOST_REGION(MISL_REGION_SHARP_CTRL, MISL_SHARP_CTRL_ADDR, MISL_SHARP_CTRL_BYTES,
                     MISL_REGION_READ | MISL_REGION_WRITE);
    MISL_HOST_REGION(MISL_REGION_SHARP_RING, MISL_SHARP_RING_ADDR, MISL_SHARP_RING_BYTES,
                     MISL_REGION_READ | MISL_REGION_WRITE);
    MISL_HOST_REGION(MISL_REGION_SHARP_OUT, MISL_SHARP_OUT_ADDR, MISL_MMIO_STRIDE,
                     MISL_REGION_READ | MISL_REGION_WRITE);
    MISL_HOST_REGION(MISL_REGION_SPRITE_ATLAS, MISL_SPRITE_ADDR, MISL_SPRITE_BYTES,
                     MISL_REGION_READ | MISL_REGION_WRITE);
    MISL_HOST_REGION(MISL_REGION_TILE_POOL, MISL_TILE_ADDR, MISL_TILE_BYTES,
                     MISL_REGION_READ | MISL_REGION_WRITE);
    MISL_HOST_REGION(MISL_REGION_TILE_MAP, MISL_TILE_MAP_ADDR, MISL_TILE_MAP_SPAN,
                     MISL_REGION_READ | MISL_REGION_WRITE);
#undef MISL_HOST_REGION
    if (!valid) {
        return 0;
    }
    memcpy(fuselage_guest_get_window_pointer(MISL_DESC_ADDR, sizeof descriptor), &descriptor,
           sizeof descriptor);
    return 1;
}
