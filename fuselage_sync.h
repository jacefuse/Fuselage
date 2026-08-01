#ifndef FUSELAGE_SYNC_H
#define FUSELAGE_SYNC_H

// Fuselage internal -- the two synchronization primitives fuselage.c needs,
// with one implementation per platform family: a plain mutex and an
// auto-reset event (one waiter wakes per signal; signaling while nobody
// waits leaves the event set so the next wait returns immediately --
// CreateEvent(bManualReset=FALSE) semantics, which the pthread version
// reproduces with a mutex + condvar + flag).
//
// Header-only on purpose: these are a handful of one-line calls on Win32 and
// barely more on pthreads; a .c file per platform would be ceremony. This is
// engine-internal -- games never see it.

#include <stdbool.h>

#if defined(_WIN32)

#include <windows.h>

typedef CRITICAL_SECTION FuselageMutex;

static inline void fuselage_mutex_init(FuselageMutex* m)    { InitializeCriticalSection(m); }
static inline void fuselage_mutex_destroy(FuselageMutex* m) { DeleteCriticalSection(m); }
static inline void fuselage_mutex_lock(FuselageMutex* m)    { EnterCriticalSection(m); }
static inline void fuselage_mutex_unlock(FuselageMutex* m)  { LeaveCriticalSection(m); }

typedef HANDLE FuselageEvent;

static inline void fuselage_event_init(FuselageEvent* e)    { *e = CreateEventW(NULL, FALSE, FALSE, NULL); }
static inline void fuselage_event_destroy(FuselageEvent* e) { if (*e) { CloseHandle(*e); *e = NULL; } }
static inline void fuselage_event_set(FuselageEvent* e)     { SetEvent(*e); }
static inline void fuselage_event_wait(FuselageEvent* e)    { WaitForSingleObject(*e, INFINITE); }

#else

#include <pthread.h>

typedef pthread_mutex_t FuselageMutex;

static inline void fuselage_mutex_init(FuselageMutex* m)    { pthread_mutex_init(m, NULL); }
static inline void fuselage_mutex_destroy(FuselageMutex* m) { pthread_mutex_destroy(m); }
static inline void fuselage_mutex_lock(FuselageMutex* m)    { pthread_mutex_lock(m); }
static inline void fuselage_mutex_unlock(FuselageMutex* m)  { pthread_mutex_unlock(m); }

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    bool            signaled;
} FuselageEvent;

static inline void fuselage_event_init(FuselageEvent* e) {
    pthread_mutex_init(&e->mutex, NULL);
    pthread_cond_init(&e->cond, NULL);
    e->signaled = false;
}

static inline void fuselage_event_destroy(FuselageEvent* e) {
    pthread_mutex_destroy(&e->mutex);
    pthread_cond_destroy(&e->cond);
}

static inline void fuselage_event_set(FuselageEvent* e) {
    pthread_mutex_lock(&e->mutex);
    e->signaled = true;
    pthread_cond_signal(&e->cond);
    pthread_mutex_unlock(&e->mutex);
}

static inline void fuselage_event_wait(FuselageEvent* e) {
    pthread_mutex_lock(&e->mutex);
    while (!e->signaled) { pthread_cond_wait(&e->cond, &e->mutex); }
    e->signaled = false;   // auto-reset: this waiter consumed the signal
    pthread_mutex_unlock(&e->mutex);
}

#endif

#endif // FUSELAGE_SYNC_H
