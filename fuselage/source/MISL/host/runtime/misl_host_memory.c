/* Prepare storage, then commit mappings after save storage is ready. */
#include "misl_host_state_internal.h"
#include "../../bootspur/misl_rom.h"
#include "../../bootspur/misl_rom_bin.h"
#include <stdlib.h>
#include <string.h>

_Static_assert(MISL_ROM_REGION_ID == MISL_REGION_BOOTSPUR, "BOOTSPUR region IDs disagree");

void fuselage_guest_release_memory(void) {
    uint8_t* released = fuselage_guest_session.memory;
    fuselage_guest_session.cpu = (VivianState){0};
    fuselage_guest_session.memory = NULL;
    fuselage_guest_session.save_span = NULL;
    fuselage_guest_session.save_byte_count = 0;
    fuselage_guest_session.plan = (MislHostImagePlan){0};
    memset(fuselage_guest_session.pixie_banks, 0, sizeof fuselage_guest_session.pixie_banks);
    memset(fuselage_guest_session.tile_banks, 0, sizeof fuselage_guest_session.tile_banks);
    memset(fuselage_guest_get_window_pointer(MISL_DESC_ADDR, sizeof(MislDescriptor)), 0,
           sizeof(MislDescriptor));
    fuselage_guest_session.phase = MISL_HOST_UNLOADED;
    free(released);
}

static uint32_t fuselage_guest_rom_word(uint32_t offset) {
    return (uint32_t)misl_rom[offset] | ((uint32_t)misl_rom[offset + 1] << 8) |
           ((uint32_t)misl_rom[offset + 2] << 16) | ((uint32_t)misl_rom[offset + 3] << 24);
}

static int fuselage_guest_valid_system_rom(void) {
    uint32_t table_end = MISL_ROM_TABLE_OFFSET + MISL_ROM_ENTRIES * 4;
    if (misl_rom_len != sizeof misl_rom || misl_rom_len > MISL_ROM_BYTES || misl_rom_len < table_end ||
        fuselage_guest_rom_word(0) != 0x544F4F42u || fuselage_guest_rom_word(4) != MISL_ROM_VERSION ||
        fuselage_guest_rom_word(8) != MISL_ROM_ENTRIES ||
        fuselage_guest_rom_word(12) != MISL_ROM_TABLE_OFFSET) {
        return 0;
    }
    for (uint32_t index = 0; index < MISL_ROM_ENTRIES; index++) {
        uint32_t offset = fuselage_guest_rom_word(MISL_ROM_TABLE_OFFSET + index * 4);
        if (offset < table_end || (offset & 1u) || (uint64_t)offset + 2 > misl_rom_len) {
            return 0;
        }
    }
    return 1;
}

int fuselage_guest_prepare_load(const MislGuestImage* image) {
    fuselage_guest_release_memory();
    fuselage_guest_session.image = image;
    fuselage_guest_session.halt = (MislGuestHaltInfo){0};
    fuselage_guest_session.halt_reason[0] = '\0';
    fuselage_guest_session.abi_declared = 0;
    fuselage_guest_session.sleep_ticks = 0;
    fuselage_guest_session.frame_estimate_us = MISL_HOST_SLICE_CEILING_US;
    fuselage_guest_session.frame_accumulated_us = 0;
    fuselage_guest_session.frame_abandoned = 0;
    fuselage_guest_session.steps_executed = 0;
    MislHostImagePlan plan;
    MislHostImageResult result =
        fuselage_guest_plan_image(image, fuselage_guest_session.memory_cap_mib, &plan);
    if (result != MISL_HOST_IMAGE_OK) {
        fuselage_guest_fail_load(MISL_GUEST_HALT_FAULT, fuselage_guest_get_image_result_message(result));
        return 0;
    }
    if (!fuselage_guest_valid_system_rom()) {
        fuselage_guest_fail_load(MISL_GUEST_HALT_FAULT, "Invalid BOOTSPUR image or entry table");
        return 0;
    }
    uint8_t* memory = calloc(plan.allocation_bytes, 1);
    if (!memory) {
        fuselage_guest_fail_load(MISL_GUEST_HALT_FAULT, "The host could not allocate the declared guest RAM");
        return 0;
    }
    memcpy(memory, image->bytes, plan.program_image_bytes);
    fuselage_guest_session.memory = memory;
    fuselage_guest_session.plan = plan;
    fuselage_guest_session.phase = MISL_HOST_PREPARED;
    return 1;
}

static void fuselage_guest_reset_graphics_pages(void) {
    memset(fuselage_guest_get_window_pointer(MISL_SPRITE_ADDR, MISL_SPRITE_BYTES), 0, MISL_SPRITE_BYTES);
    memset(fuselage_guest_get_window_pointer(MISL_TILE_ADDR, MISL_TILE_BYTES), 0, MISL_TILE_BYTES);
    memset(fuselage_guest_get_window_pointer(MISL_STATUS_ADDR, MISL_STATUS_BYTES), 0, MISL_STATUS_BYTES);
    memset(fuselage_guest_get_window_pointer(MISL_PIXIE_LIST_ADDR, MISL_PIXIE_LIST_BYTES), 0,
           MISL_PIXIE_LIST_BYTES);
    MislStatus* status = fuselage_guest_get_window_pointer(MISL_STATUS_ADDR, sizeof(MislStatus));
    status->pixieList.firstRejectIndex = MISL_PIXIE_NO_REJECT;
    memset(fuselage_guest_session.sprite_dirty, 0, sizeof fuselage_guest_session.sprite_dirty);
    memset(fuselage_guest_session.tile_dirty, 0, sizeof fuselage_guest_session.tile_dirty);
    memset(fuselage_guest_session.tile_map_dirty, 0, sizeof fuselage_guest_session.tile_map_dirty);
    fuselage_guest_session.sprites_dirty = 0;
    fuselage_guest_session.tiles_dirty = 0;
    fuselage_guest_session.tile_maps_dirty = 0;
    memset(fuselage_guest_session.tile_writable, 1, sizeof fuselage_guest_session.tile_writable);
}

int fuselage_guest_commit_load(const uint8_t* save_span, uint32_t save_byte_count) {
    if (fuselage_guest_session.phase != MISL_HOST_PREPARED) {
        return 0;
    }
    const MislHostImagePlan* plan = &fuselage_guest_session.plan;
    if ((save_byte_count && !save_span) || save_byte_count > plan->save_bytes ||
        (plan->save_bytes && !save_byte_count) || (!save_byte_count && save_span)) {
        fuselage_guest_fail_load(MISL_GUEST_HALT_SRAM,
                                 "Save storage does not match its declared reservation");
        return 0;
    }
    fuselage_guest_reset_graphics_pages();
    fuselage_guest_session.save_span = save_span;
    fuselage_guest_session.save_byte_count = save_byte_count;
    VivianState* cpu = &fuselage_guest_session.cpu;
    VIVIAN_Init(cpu, fuselage_guest_session.memory, plan->allocation_bytes, plan->entry_offset);
    cpu->regs[2] = plan->program_bytes;
    VIVIAN_MapWindow(cpu, MISL_MMIO_BASE, fuselage_guest_session.window, MISL_MMIO_BYTES);
    VIVIAN_MapBanks(cpu, 0, MISL_PIXIE_RAM_ADDR, MISL_PIXIE_RAM_SHIFT, MISL_PIXIE_RAM_STRIDE,
                    MISL_PIXIE_RAM_BANKS, fuselage_guest_session.pixie_banks, NULL);
    VIVIAN_MapBanks(cpu, 1, MISL_TILE_MAP_ADDR, MISL_TILE_MAP_SHIFT, MISL_TILE_MAP_BYTES, MISL_TILE_MAP_BANKS,
                    fuselage_guest_session.tile_banks, fuselage_guest_session.tile_writable);
    VIVIAN_MapReadOnlyMemory(cpu, plan->asset_base, plan->asset_bytes, plan->asset_byte_count);
    VIVIAN_MapReadOnlySpan(cpu, MISL_HOST_BOOTSPUR_SPAN, MISL_ROM_ADDR, misl_rom, misl_rom_len);
    VIVIAN_MapReadOnlySpan(cpu, MISL_HOST_SAVE_SPAN, plan->save_base, save_span, save_byte_count);
    if (!fuselage_guest_publish_descriptor()) {
        fuselage_guest_fail_load(MISL_GUEST_HALT_FAULT, "The mapped regions do not fit the guest descriptor");
        return 0;
    }
    fuselage_guest_session.phase = MISL_HOST_RUNNING;
    return 1;
}
