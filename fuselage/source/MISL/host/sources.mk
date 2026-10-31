# Native host core; launcher and embedded ownership are selected separately.
MISL_HOST_SOURCES := \
    source/MISL/host/runtime/misl_host_abi.c \
    source/MISL/host/services/misl_host_audio.c \
    source/MISL/host/services/misl_host_audio_pump.c \
    source/MISL/host/services/misl_host_clock.c \
    source/MISL/host/graphics/misl_host_colors.c \
    source/MISL/host/runtime/misl_host_descriptor.c \
    source/MISL/host/services/misl_host_diagnostics.c \
    source/MISL/host/services/misl_host_dice.c \
    source/MISL/host/runtime/misl_host_ecall.c \
    source/MISL/host/graphics/misl_host_graphics_banks.c \
    source/MISL/host/graphics/misl_host_graphics_buffers.c \
    source/MISL/host/graphics/misl_host_graphics_watch.c \
    source/MISL/host/runtime/misl_host_halt.c \
    source/MISL/host/runtime/misl_host_image.c \
    source/MISL/host/services/misl_host_input.c \
    source/MISL/host/services/misl_host_input_pump.c \
    source/MISL/host/graphics/misl_host_interactions.c \
    source/MISL/host/runtime/misl_host_memory.c \
    source/MISL/host/runtime/misl_host_pacing.c \
    source/MISL/host/runtime/misl_host_pages.c \
    source/MISL/host/graphics/misl_host_pixies.c \
    source/MISL/host/runtime/misl_host_pointers.c \
    source/MISL/host/runtime/misl_host_presentation.c \
    source/MISL/host/runtime/misl_host_pumps.c \
    source/MISL/host/services/misl_host_rom.c \
    source/MISL/host/services/misl_host_rom_diagnostics.c \
    source/MISL/host/services/misl_host_rom_memory.c \
    source/MISL/host/services/misl_host_rom_save.c \
    source/MISL/host/runtime/misl_host_save.c \
    source/MISL/host/runtime/misl_host_scheduler.c \
    source/MISL/host/graphics/misl_host_sprites.c \
    source/MISL/host/runtime/misl_host_state.c \
    source/MISL/host/services/misl_host_system.c \
    source/MISL/host/graphics/misl_host_text.c \
    source/MISL/host/graphics/misl_host_tiles.c
MISL_LAUNCHER_SOURCES := \
    source/MISL/host/launcher/misl_launcher.c \
    source/MISL/host/launcher/misl_launcher_empty.c \
    source/MISL/host/launcher/misl_launcher_image.c \
    source/MISL/host/launcher/misl_launcher_manifest.c \
    source/MISL/host/launcher/misl_launcher_options.c \
    source/MISL/host/launcher/misl_launcher_report.c \
    source/MISL/host/launcher/misl_launcher_tick.c
