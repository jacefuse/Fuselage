/* Persistence integration supplies these operations; no file I/O in dispatch. */
#ifndef MISL_HOST_SAVE_INTERNAL_H
#define MISL_HOST_SAVE_INTERNAL_H
#include <stdbool.h>
#include <stdint.h>

/* Validate the full range before changing save storage; consume input synchronously. */
bool fuselage_guest_write_save(uint32_t offset, const void* input, uint32_t byte_count);
uint32_t fuselage_guest_get_save_size(void);
/* PREPARED only; mappings are committed after storage is ready. */
int fuselage_guest_commit_save_load(void);
/* Flush or abandon without freeing storage still borrowed by CPU mappings. */
bool fuselage_guest_finish_save(bool clean_exit);
/* Detach guest mappings before closing. Returns false while they borrow storage. */
bool fuselage_guest_close_save(void);
#endif
