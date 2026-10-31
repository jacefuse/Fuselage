/* Private POSIX backend worker. SEE: (SHARP_MACOS.md, POSIX threads) */
#if defined(__linux__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include "../../backend/sharp_thread.h"
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#if defined(__APPLE__)
#include <pthread/qos.h>
#endif

struct SharpThread {
    pthread_t handle;
    void (*function)(void*);
    void* user;
};
static void* sharp_thread_trampoline(void* context) {
    SharpThread* thread = context;
    thread->function(thread->user);
    return NULL;
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
    pthread_attr_t attributes;
    pthread_attr_t* selected_attributes = NULL;
    if (pthread_attr_init(&attributes) == 0) {
        selected_attributes = &attributes;
#if defined(__APPLE__)
        /* Render-ahead work uses a QoS hint, not a workgroup deadline. */
        pthread_attr_set_qos_class_np(&attributes, QOS_CLASS_USER_INITIATED, 0);
#endif
    }
    int result = pthread_create(&thread->handle, selected_attributes, sharp_thread_trampoline, thread);
    if (selected_attributes) {
        pthread_attr_destroy(selected_attributes);
    }
    if (result != 0) {
        free(thread);
        return NULL;
    }
    return thread;
}
void sharp_thread_join(SharpThread* thread) {
    if (!thread) {
        return;
    }
    /* Preserve the record on failure; the caller still needs an error policy. */
    if (pthread_join(thread->handle, NULL) == 0) {
        free(thread);
    }
}
void sharp_thread_sleep_milliseconds(uint32_t milliseconds) {
    struct timespec remaining = {milliseconds / 1000, (long)(milliseconds % 1000) * 1000000L};
    while (nanosleep(&remaining, &remaining) != 0 && errno == EINTR) {
    }
}
