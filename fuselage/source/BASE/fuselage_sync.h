#ifndef FUSELAGE_SYNC_H
#define FUSELAGE_SYNC_H

// Internal mutexes and auto-reset events shared by engine systems.
// SEE: (FUSELAGE_SYNC.md, Synchronization contract)

#include <stdbool.h>

#if defined(_WIN32)

#include <windows.h>

typedef CRITICAL_SECTION FuselageMutex;

static inline void fuselage_mutex_init(FuselageMutex* mutex) {
    InitializeCriticalSection(mutex);
}
static inline void fuselage_mutex_destroy(FuselageMutex* mutex) {
    DeleteCriticalSection(mutex);
}
static inline void fuselage_mutex_lock(FuselageMutex* mutex) {
    EnterCriticalSection(mutex);
}
static inline void fuselage_mutex_unlock(FuselageMutex* mutex) {
    LeaveCriticalSection(mutex);
}

typedef HANDLE FuselageEvent;

static inline void fuselage_event_init(FuselageEvent* event) {
    *event = CreateEventW(NULL, FALSE, FALSE, NULL);
}
static inline void fuselage_event_destroy(FuselageEvent* event) {
    if (*event) {
        CloseHandle(*event);
        *event = NULL;
    }
}
static inline void fuselage_event_set(FuselageEvent* event) {
    SetEvent(*event);
}
static inline void fuselage_event_wait(FuselageEvent* event) {
    WaitForSingleObject(*event, INFINITE);
}
static inline bool fuselage_mutex_init_checked(FuselageMutex* mutex) {
    return InitializeCriticalSectionAndSpinCount(mutex, 0) != 0;
}
static inline bool fuselage_event_init_checked(FuselageEvent* event) {
    fuselage_event_init(event);
    return *event != NULL;
}

#else

#include <pthread.h>

// Tickets prevent a releasing thread from overtaking an already queued waiter.
// SEE: (FUSELAGE_SYNC.md, Mutex behavior)
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    unsigned next_ticket;    // Next ticket to issue.
    unsigned serving_ticket; // Ticket allowed to enter.
} FuselageMutex;

static inline void fuselage_mutex_init(FuselageMutex* mutex) {
    pthread_mutex_init(&mutex->mutex, NULL);
    pthread_cond_init(&mutex->condition, NULL);
    mutex->next_ticket = 0;
    mutex->serving_ticket = 0;
}

static inline void fuselage_mutex_destroy(FuselageMutex* mutex) {
    pthread_mutex_destroy(&mutex->mutex);
    pthread_cond_destroy(&mutex->condition);
}

static inline void fuselage_mutex_lock(FuselageMutex* mutex) {
    pthread_mutex_lock(&mutex->mutex);
    unsigned ticket = mutex->next_ticket++;
    // Recheck after broadcasts and spurious wakeups.
    while (ticket != mutex->serving_ticket) {
        pthread_cond_wait(&mutex->condition, &mutex->mutex);
    }
    pthread_mutex_unlock(&mutex->mutex);
}

static inline void fuselage_mutex_unlock(FuselageMutex* mutex) {
    pthread_mutex_lock(&mutex->mutex);
    mutex->serving_ticket++;
    pthread_cond_broadcast(&mutex->condition);
    pthread_mutex_unlock(&mutex->mutex);
}

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    bool signaled;
} FuselageEvent;

static inline void fuselage_event_init(FuselageEvent* event) {
    pthread_mutex_init(&event->mutex, NULL);
    pthread_cond_init(&event->condition, NULL);
    event->signaled = false;
}

static inline void fuselage_event_destroy(FuselageEvent* event) {
    pthread_mutex_destroy(&event->mutex);
    pthread_cond_destroy(&event->condition);
}

static inline void fuselage_event_set(FuselageEvent* event) {
    pthread_mutex_lock(&event->mutex);
    event->signaled = true;
    pthread_cond_signal(&event->condition);
    pthread_mutex_unlock(&event->mutex);
}

static inline void fuselage_event_wait(FuselageEvent* event) {
    pthread_mutex_lock(&event->mutex);
    while (!event->signaled) {
        pthread_cond_wait(&event->condition, &event->mutex);
    }
    event->signaled = false; // auto-reset: this waiter consumed the signal
    pthread_mutex_unlock(&event->mutex);
}
static inline bool fuselage_mutex_init_checked(FuselageMutex* mutex) {
    if (pthread_mutex_init(&mutex->mutex, NULL)) {
        return false;
    }
    if (pthread_cond_init(&mutex->condition, NULL)) {
        pthread_mutex_destroy(&mutex->mutex);
        return false;
    }
    mutex->next_ticket = mutex->serving_ticket = 0;
    return true;
}
static inline bool fuselage_event_init_checked(FuselageEvent* event) {
    if (pthread_mutex_init(&event->mutex, NULL)) {
        return false;
    }
    if (pthread_cond_init(&event->condition, NULL)) {
        pthread_mutex_destroy(&event->mutex);
        return false;
    }
    event->signaled = false;
    return true;
}

#endif

#endif // FUSELAGE_SYNC_H
