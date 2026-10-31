/* Save-region storage. SEE: (SRAM.md, Lifecycle) */
#ifndef FUSELAGE_SRAM_H
#define FUSELAGE_SRAM_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define SRAM_VERSION "0.1.2026103001 ENTERON"
#define SRAM_ID_BYTES 16u
#ifdef __cplusplus
extern "C" {
#endif
uint32_t SRAM_GetSize(void);
bool SRAM_IsEnabled(void);
bool SRAM_IsDirty(void);
/* Refuse zero length, null storage and incomplete ranges without changing bytes. */
bool SRAM_Read(uint32_t offset, void* output, uint32_t byte_count);
bool SRAM_Write(uint32_t offset, const void* input, uint32_t byte_count);
/* Retires an open working copy without saving; borrowers must detach first. */
bool SRAM_Configure(const char* id, uint32_t version, uint32_t byte_count);
/* Overlong paths are refused rather than truncated. NULL/empty selects beside-program storage. */
bool SRAM_SetSaveDirectory(const char* directory);
bool SRAM_Open(const char* program_path);
const char* SRAM_GetFailureReason(void);
const char* SRAM_GetPath(void);
void SRAM_Abandon(void);
/* True also means no work: unopened, abandoned or clean. False leaves dirty state. */
bool SRAM_Save(void);
/* Does not save; invalidates borrowed storage. */
void SRAM_Close(void);
bool SRAM_GetProgramPath(char* output, size_t capacity, const char* argv0);
#ifdef __cplusplus
}
#endif
#endif
