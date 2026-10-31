/* Native failure presentation; no UI dependency in SRAM itself. */
#ifndef FUSELAGE_SAVE_H
#define FUSELAGE_SAVE_H
#ifdef __cplusplus
extern "C" {
#endif
void FuselageTickSaveFailure(const char* reason);
#ifdef __cplusplus
}
#endif
#endif
