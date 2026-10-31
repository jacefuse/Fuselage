#pragma once

// Standard runtime entry names. SEE: (MISL_RUNTIME.md, Heap break)
#ifdef __cplusplus
extern "C" {
#endif

// Return the previous break, or (void*)-1 on refusal. No allocator is supplied.
void* _sbrk(long increment);
void* sbrk(long increment);

#ifdef __cplusplus
}
#endif
