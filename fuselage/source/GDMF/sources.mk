# Explicit shipping sources. SEE: (FUSELAGE_BUILD.md, Source inventories)
GDMF_COMMON_SOURCES := \
    compute/gdmf_compute.c \
    core/gdmf.c \
    colors/gdmf_colors.c \
    workers/gdmf_workers.c \
    vulkan/compute/gdmf_compute_vulkan.c \
    vulkan/compute/gdmf_compute_vulkan_record.c \
    vulkan/renderer/gdmf_vulkan.c \
    vulkan/renderer/capture/gdmf_vulkan_capture_ppm.c \
    vulkan/renderer/capture/gdmf_vulkan_capture_readback.c \
    vulkan/renderer/capture/gdmf_vulkan_capture_requests.c \
    vulkan/renderer/resources/gdmf_vulkan_commands.c \
    vulkan/renderer/composition/gdmf_vulkan_composition.c \
    vulkan/renderer/device/gdmf_vulkan_device.c \
    vulkan/renderer/frame/gdmf_vulkan_frame.c \
    vulkan/renderer/composition/gdmf_vulkan_geometry.c \
    vulkan/renderer/device/gdmf_vulkan_instance.c \
    vulkan/renderer/masks/gdmf_vulkan_mask_targets.c \
    vulkan/renderer/masks/gdmf_vulkan_masks.c \
    vulkan/renderer/resources/gdmf_vulkan_memory.c \
    vulkan/renderer/palettes/gdmf_vulkan_palettes.c \
    vulkan/renderer/device/gdmf_vulkan_physical_device.c \
    vulkan/renderer/presentation/gdmf_vulkan_render_pass.c \
    vulkan/renderer/resources/gdmf_vulkan_retirement.c \
    vulkan/renderer/resources/gdmf_vulkan_staging.c \
    vulkan/renderer/presentation/gdmf_vulkan_swapchain.c \
    vulkan/renderer/resources/gdmf_vulkan_uploads.c \
    pixies/gdmf_pixies_attributes.c \
    pixies/gdmf_pixies_commands.c \
    pixies/gdmf_pixies_compression.c \
    pixies/gdmf_pixies_decode.c \
    pixies/gdmf_pixies_decode_queue.c \
    pixies/gdmf_pixies_diagnostics.c \
    pixies/gdmf_pixies_huffman.c \
    pixies/gdmf_pixies_live.c \
    pixies/gdmf_pixies_mask_decode.c \
    pixies/gdmf_pixies_packing.c \
    pixies/gdmf_pixies_raster.c \
    pixies/gdmf_pixies_state.c \
    pixies/gdmf_pixies_transforms.c \
    pixies/gdmf_pixies_unpack_command.c \
    vulkan/pixies/gdmf_pixies_backend.c \
    vulkan/pixies/gdmf_pixies_decode_vulkan.c \
    vulkan/pixies/gdmf_pixies_frame_geometry.c \
    vulkan/pixies/gdmf_pixies_frame_prepare.c \
    vulkan/pixies/gdmf_pixies_frame_record.c \
    vulkan/pixies/gdmf_pixies_image_uploads.c \
    vulkan/pixies/gdmf_pixies_images.c \
    vulkan/pixies/gdmf_pixies_pipelines.c \
    vulkan/pixies/gdmf_pixies_vertex_buffers.c \
    sprites/gdmf_sprites_state.c \
    sprites/gdmf_sprites_bitmaps.c \
    sprites/gdmf_sprites_attributes.c \
    sprites/gdmf_sprites_help.c \
    sprites/gdmf_sprites_geometry.c \
    sprites/gdmf_sprites_probe.c \
    sprites/gdmf_sprites_collisions.c \
    sprites/gdmf_sprites_collision_pair.c \
    sprites/gdmf_sprites_collision_batch.c \
    sprites/gdmf_sprites_collision_search.c \
    vulkan/sprites/gdmf_sprites_atlas.c \
    vulkan/sprites/gdmf_sprites_atlas_uploads.c \
    vulkan/sprites/gdmf_sprites_descriptors.c \
    vulkan/sprites/gdmf_sprites_pipeline.c \
    vulkan/sprites/gdmf_sprites_vertex_buffers.c \
    vulkan/sprites/gdmf_sprites_frame_geometry.c \
    vulkan/sprites/gdmf_sprites_frame_prepare.c \
    vulkan/sprites/gdmf_sprites_frame_record.c \
    vulkan/sprites/gdmf_sprites_backend.c \
    tiles/gdmf_tiles_attributes.c \
    tiles/gdmf_tiles_bitmaps.c \
    tiles/gdmf_tiles_cells.c \
    tiles/gdmf_tiles_help.c \
    tiles/gdmf_tiles_patterns.c \
    tiles/gdmf_tiles_probe.c \
    tiles/gdmf_tiles_queries.c \
    tiles/gdmf_tiles_scroll.c \
    tiles/gdmf_tiles_state.c \
    vulkan/tiles/gdmf_tiles_atlas.c \
    vulkan/tiles/gdmf_tiles_atlas_uploads.c \
    vulkan/tiles/gdmf_tiles_backend.c \
    vulkan/tiles/gdmf_tiles_descriptors.c \
    vulkan/tiles/gdmf_tiles_frame_geometry.c \
    vulkan/tiles/gdmf_tiles_frame_plan.c \
    vulkan/tiles/gdmf_tiles_frame_prepare.c \
    vulkan/tiles/gdmf_tiles_frame_record.c \
    vulkan/tiles/gdmf_tiles_pipeline.c \
    vulkan/tiles/gdmf_tiles_vertex_buffers.c \
    textlayer/gdmf_textlayer_state.c \
    textlayer/gdmf_textlayer_print.c \
    textlayer/gdmf_textlayer_format.c \
    textlayer/gdmf_textlayer_font.c \
    textlayer/gdmf_textlayer_font_data.c \
    vulkan/textlayer/gdmf_textlayer_atlas.c \
    vulkan/textlayer/gdmf_textlayer_cell_buffers.c \
    vulkan/textlayer/gdmf_textlayer_descriptors.c \
    vulkan/textlayer/gdmf_textlayer_pipeline.c \
    vulkan/textlayer/gdmf_textlayer_frame.c \
    vulkan/textlayer/gdmf_textlayer_backend.c \
    interactions/gdmf_interactions_batch.c \
    interactions/gdmf_interactions_cells.c \
    interactions/gdmf_interactions_collision.c \
    interactions/gdmf_interactions_geometry.c

GDMF_WIN32_SOURCES := \
    window/gdmf_window_win32.c \
    vulkan/surface/gdmf_surface_win32.c

GDMF_MACOS_SOURCES := \
    window/gdmf_window_macos.c \
    window/gdmf_window_macos_appkit.m \
    window/gdmf_window_macos_presentation.m \
    vulkan/surface/gdmf_surface_macos.c

GDMF_LINUX_SOURCES := \
    window/gdmf_window_linux.c \
    window/gdmf_window_wayland.c \
    window/gdmf_window_x11.c \
    vulkan/surface/gdmf_surface_linux.c \
    vulkan/surface/gdmf_surface_wayland.c \
    vulkan/surface/gdmf_surface_x11.c

