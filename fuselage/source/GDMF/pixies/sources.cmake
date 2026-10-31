# Pixies implementation files; renderer, compute and Colors are supplied by the engine.
set(GDMF_PIXIES_HOST_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/gdmf_pixies_attributes.c"
    "${CMAKE_CURRENT_LIST_DIR}/gdmf_pixies_commands.c"
    "${CMAKE_CURRENT_LIST_DIR}/gdmf_pixies_compression.c"
    "${CMAKE_CURRENT_LIST_DIR}/gdmf_pixies_decode.c"
    "${CMAKE_CURRENT_LIST_DIR}/gdmf_pixies_decode_queue.c"
    "${CMAKE_CURRENT_LIST_DIR}/gdmf_pixies_diagnostics.c"
    "${CMAKE_CURRENT_LIST_DIR}/gdmf_pixies_huffman.c"
    "${CMAKE_CURRENT_LIST_DIR}/gdmf_pixies_live.c"
    "${CMAKE_CURRENT_LIST_DIR}/gdmf_pixies_mask_decode.c"
    "${CMAKE_CURRENT_LIST_DIR}/gdmf_pixies_packing.c"
    "${CMAKE_CURRENT_LIST_DIR}/gdmf_pixies_raster.c"
    "${CMAKE_CURRENT_LIST_DIR}/gdmf_pixies_state.c"
    "${CMAKE_CURRENT_LIST_DIR}/gdmf_pixies_transforms.c"
    "${CMAKE_CURRENT_LIST_DIR}/gdmf_pixies_unpack_command.c"
)

set(GDMF_PIXIES_VULKAN_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/../vulkan/pixies/gdmf_pixies_backend.c"
    "${CMAKE_CURRENT_LIST_DIR}/../vulkan/pixies/gdmf_pixies_decode_vulkan.c"
    "${CMAKE_CURRENT_LIST_DIR}/../vulkan/pixies/gdmf_pixies_frame_geometry.c"
    "${CMAKE_CURRENT_LIST_DIR}/../vulkan/pixies/gdmf_pixies_frame_prepare.c"
    "${CMAKE_CURRENT_LIST_DIR}/../vulkan/pixies/gdmf_pixies_frame_record.c"
    "${CMAKE_CURRENT_LIST_DIR}/../vulkan/pixies/gdmf_pixies_image_uploads.c"
    "${CMAKE_CURRENT_LIST_DIR}/../vulkan/pixies/gdmf_pixies_images.c"
    "${CMAKE_CURRENT_LIST_DIR}/../vulkan/pixies/gdmf_pixies_pipelines.c"
    "${CMAKE_CURRENT_LIST_DIR}/../vulkan/pixies/gdmf_pixies_vertex_buffers.c"
)

set(GDMF_PIXIES_SHADER_HEADERS
    "${CMAKE_CURRENT_LIST_DIR}/../shaders/pixie_vert.h"
    "${CMAKE_CURRENT_LIST_DIR}/../shaders/pixie_frag.h"
    "${CMAKE_CURRENT_LIST_DIR}/../shaders/pixie_live_vert.h"
    "${CMAKE_CURRENT_LIST_DIR}/../shaders/pixie_live_frag.h"
)
