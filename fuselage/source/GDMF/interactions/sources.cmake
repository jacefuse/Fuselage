# Interactions implementation; Sprite/Tile host services and workers are supplied by GDMF.
set(GDMF_INTERACTIONS_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/gdmf_interactions_batch.c"
    "${CMAKE_CURRENT_LIST_DIR}/gdmf_interactions_cells.c"
    "${CMAKE_CURRENT_LIST_DIR}/gdmf_interactions_collision.c"
    "${CMAKE_CURRENT_LIST_DIR}/gdmf_interactions_geometry.c"
)
