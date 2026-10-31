/* Private platform thread helpers. SEE: (SHARP.md, Platform threads) */
#ifndef SHARP_THREAD_H
#define SHARP_THREAD_H
#include <stdint.h>
typedef struct SharpThread SharpThread;
SharpThread* sharp_thread_start(void (*function)(void*), void* user);
void sharp_thread_join(SharpThread* thread);
void sharp_thread_sleep_milliseconds(uint32_t milliseconds);
#endif
