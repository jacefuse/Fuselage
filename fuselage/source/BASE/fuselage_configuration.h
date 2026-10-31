/* Native pre-init configuration, shared by project and launcher entry points. */
#ifndef FUSELAGE_CONFIGURATION_H
#define FUSELAGE_CONFIGURATION_H
#include <stdbool.h>
#define FUSELAGE_VERSION "0.5.2026103001 ENTERON"
#ifdef __cplusplus
extern "C" {
#endif
void FuselageSetTitle(const char* title);
void FuselageSetResolution(int width, int height);
void FuselageSetCanvasResolution(int width, int height);
void FuselageSetAspectRatio(int numerator, int denominator);
void FuselageSetWindowIcon(int width, int height, const unsigned char* rgba);
void FuselageSetSimRate(double rate);
void FuselageSetBanner(bool enabled);
void FuselageSetAudioPoolMB(int pool_mib);
typedef void (*FuselageTickCallback)(void);
void FuselageSetTickCallback(FuselageTickCallback callback);
#ifdef __cplusplus
}
#endif
#endif
