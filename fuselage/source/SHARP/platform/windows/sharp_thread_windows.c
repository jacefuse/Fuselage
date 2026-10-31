#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include "../../backend/sharp_thread.h"

struct SharpThread {
    HANDLE handle;
    void (*function)(void*);
    void* user;
};
static DWORD WINAPI sharp_thread_trampoline(LPVOID context) {
    SharpThread* thread = context;
    thread->function(thread->user);
    return 0;
}
SharpThread* sharp_thread_start(void (*function)(void*), void* user) {
    if (!function) {
        return NULL;
    }
    SharpThread* thread = calloc(1, sizeof *thread);
    if (!thread) {
        return NULL;
    }
    thread->function = function;
    thread->user = user;
    thread->handle = CreateThread(NULL, 0, sharp_thread_trampoline, thread, 0, NULL);
    if (!thread->handle) {
        free(thread);
        return NULL;
    }
    /* Scheduling hint, not an audio deadline. */
    SetThreadPriority(thread->handle, THREAD_PRIORITY_ABOVE_NORMAL);
    return thread;
}
void sharp_thread_join(SharpThread* thread) {
    if (!thread) {
        return;
    }
    WaitForSingleObject(thread->handle, INFINITE);
    CloseHandle(thread->handle);
    free(thread);
}
void sharp_thread_sleep_milliseconds(uint32_t milliseconds) {
    Sleep(milliseconds);
}
