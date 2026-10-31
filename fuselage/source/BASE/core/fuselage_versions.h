/* Shared native identity list for the launcher and startup banner. */
#ifndef FUSELAGE_VERSIONS_H
#define FUSELAGE_VERSIONS_H
#include <stddef.h>
typedef struct {
    const char* name;
    const char* version;
} FuselageComponentVersion;
const FuselageComponentVersion* fuselage_get_component_versions(size_t* count);
#endif
