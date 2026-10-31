// Synchronous worker pool. SEE: (GDMF_WORKERS.md, Job contract)
#if defined(__linux__)
#define _POSIX_C_SOURCE 200809L
#endif

#include "gdmf_workers.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define GDMF_WORKERS_MAX 64

#if defined(_WIN32)
#include <windows.h>

typedef HANDLE gdmf_worker_thread;
static SRWLOCK gdmf_workers_mutex = SRWLOCK_INIT;
static CONDITION_VARIABLE gdmf_workers_start_condition = CONDITION_VARIABLE_INIT;
static CONDITION_VARIABLE gdmf_workers_done_condition = CONDITION_VARIABLE_INIT;
static void gdmf_workers_lock(void) {
    AcquireSRWLockExclusive(&gdmf_workers_mutex);
}
static void gdmf_workers_unlock(void) {
    ReleaseSRWLockExclusive(&gdmf_workers_mutex);
}
static void gdmf_workers_wait_start(void) {
    SleepConditionVariableSRW(&gdmf_workers_start_condition, &gdmf_workers_mutex, INFINITE, 0);
}
static void gdmf_workers_wait_done(void) {
    SleepConditionVariableSRW(&gdmf_workers_done_condition, &gdmf_workers_mutex, INFINITE, 0);
}
static void gdmf_workers_wake_start(void) {
    WakeAllConditionVariable(&gdmf_workers_start_condition);
}
static void gdmf_workers_wake_done(void) {
    WakeConditionVariable(&gdmf_workers_done_condition);
}
static int gdmf_workers_logical_cpus(void) {
    SYSTEM_INFO system_information;
    GetSystemInfo(&system_information);
    return system_information.dwNumberOfProcessors > GDMF_WORKERS_MAX
               ? GDMF_WORKERS_MAX
               : (int)system_information.dwNumberOfProcessors;
}

static void gdmf_workers_main(int worker_index);
static DWORD WINAPI gdmf_workers_trampoline(LPVOID context) {
    gdmf_workers_main((int)(intptr_t)context);
    return 0;
}
static bool gdmf_workers_thread_start(gdmf_worker_thread* thread, int worker_index) {
    *thread = CreateThread(NULL, 0, gdmf_workers_trampoline, (LPVOID)(intptr_t)worker_index, 0, NULL);
    return *thread != NULL;
}
static void gdmf_workers_thread_join(gdmf_worker_thread thread) {
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
}

#else
#include <pthread.h>
#include <unistd.h>

typedef pthread_t gdmf_worker_thread;
static pthread_mutex_t gdmf_workers_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gdmf_workers_start_condition = PTHREAD_COND_INITIALIZER;
static pthread_cond_t gdmf_workers_done_condition = PTHREAD_COND_INITIALIZER;
static void gdmf_workers_lock(void) {
    pthread_mutex_lock(&gdmf_workers_mutex);
}
static void gdmf_workers_unlock(void) {
    pthread_mutex_unlock(&gdmf_workers_mutex);
}
static void gdmf_workers_wait_start(void) {
    pthread_cond_wait(&gdmf_workers_start_condition, &gdmf_workers_mutex);
}
static void gdmf_workers_wait_done(void) {
    pthread_cond_wait(&gdmf_workers_done_condition, &gdmf_workers_mutex);
}
static void gdmf_workers_wake_start(void) {
    pthread_cond_broadcast(&gdmf_workers_start_condition);
}
static void gdmf_workers_wake_done(void) {
    pthread_cond_signal(&gdmf_workers_done_condition);
}
static int gdmf_workers_logical_cpus(void) {
    long logical_count = sysconf(_SC_NPROCESSORS_ONLN);
    return logical_count > GDMF_WORKERS_MAX ? GDMF_WORKERS_MAX : (logical_count > 0 ? (int)logical_count : 1);
}

static void gdmf_workers_main(int worker_index);
static void* gdmf_workers_trampoline(void* context) {
    gdmf_workers_main((int)(intptr_t)context);
    return NULL;
}
static bool gdmf_workers_thread_start(gdmf_worker_thread* thread, int worker_index) {
    return pthread_create(thread, NULL, gdmf_workers_trampoline, (void*)(intptr_t)worker_index) == 0;
}
static void gdmf_workers_thread_join(gdmf_worker_thread thread) {
    pthread_join(thread, NULL);
}
#endif

static gdmf_worker_thread gdmf_workers_handles[GDMF_WORKERS_MAX - 1];
static int gdmf_workers_count;
static bool gdmf_workers_started;
static bool gdmf_workers_quit;
static bool gdmf_workers_busy;
static unsigned gdmf_workers_generation;
static int gdmf_workers_pending;
static int gdmf_workers_participants;
static int gdmf_workers_limit;

static gdmf_work_callback gdmf_workers_callback;
static void* gdmf_workers_context;
static int gdmf_workers_item_count, gdmf_workers_chunk;
static atomic_int gdmf_workers_next;

static _Thread_local bool gdmf_workers_in_job;

static void gdmf_workers_inline(int count, gdmf_work_callback callback, void* context) {
    bool already_in_job = gdmf_workers_in_job;
    gdmf_workers_in_job = true;
    callback(0, count, context);
    gdmf_workers_in_job = already_in_job;
}

static void gdmf_workers_work(void) {
    const bool already_in_job = gdmf_workers_in_job;
    gdmf_workers_in_job = true;
    for (;;) {
        int begin = atomic_load(&gdmf_workers_next);
        if (begin >= gdmf_workers_item_count) {
            break;
        }
        const int end = (gdmf_workers_item_count - begin < gdmf_workers_chunk) ? gdmf_workers_item_count
                                                                               : begin + gdmf_workers_chunk;
        // Reserve only valid indices; an overshooting fetch-add could wrap at INT_MAX.
        if (!atomic_compare_exchange_weak(&gdmf_workers_next, &begin, end)) {
            continue;
        }
        gdmf_workers_callback(begin, end, gdmf_workers_context);
    }
    gdmf_workers_in_job = already_in_job;
}

static void gdmf_workers_main(int worker_index) {
    unsigned seen_generation = 0;
    gdmf_workers_lock();
    for (;;) {
        while (gdmf_workers_generation == seen_generation && !gdmf_workers_quit) {
            gdmf_workers_wait_start();
        }
        if (gdmf_workers_quit) {
            break;
        }
        seen_generation = gdmf_workers_generation;
        const bool participates = worker_index < gdmf_workers_participants - 1;
        gdmf_workers_unlock();

        if (participates) {
            gdmf_workers_work();
        }

        gdmf_workers_lock();
        if (--gdmf_workers_pending == 0) {
            gdmf_workers_wake_done();
        }
    }
    gdmf_workers_unlock();
}

static int gdmf_workers_wanted_threads(void) {
    int threads = gdmf_workers_limit > 0 ? gdmf_workers_limit : gdmf_workers_logical_cpus();
    if (threads < 1) {
        threads = 1;
    }
    if (threads > GDMF_WORKERS_MAX) {
        threads = GDMF_WORKERS_MAX;
    }
    return threads;
}

static void gdmf_workers_start_locked(void) {
    if (gdmf_workers_started) {
        return;
    }
    gdmf_workers_started = true;
    int wanted_workers = gdmf_workers_logical_cpus() - 1;
    if (wanted_workers > GDMF_WORKERS_MAX - 1) {
        wanted_workers = GDMF_WORKERS_MAX - 1;
    }
    gdmf_workers_generation = 0;
    for (int worker_index = 0; worker_index < wanted_workers; worker_index++) {
        if (!gdmf_workers_thread_start(&gdmf_workers_handles[gdmf_workers_count], gdmf_workers_count)) {
            fprintf(stderr, "[GDMF] Worker %d could not start; %d workers available\n", worker_index,
                    gdmf_workers_count);
            break;
        }
        gdmf_workers_count++;
    }
}

void gdmf_workers_run(int count, int chunk, gdmf_work_callback callback, void* context) {
    if (count <= 0 || !callback) {
        return;
    }
    if (chunk < 1) {
        chunk = 1;
    }

    if (gdmf_workers_in_job) {
        gdmf_workers_inline(count, callback, context);
        return;
    }

    gdmf_workers_lock();
    const int threads = gdmf_workers_wanted_threads();
    if (threads <= 1 || gdmf_workers_busy) {
        gdmf_workers_unlock();
        gdmf_workers_inline(count, callback, context);
        return;
    }
    gdmf_workers_start_locked();
    gdmf_workers_busy = true;
    gdmf_workers_callback = callback;
    gdmf_workers_context = context;
    gdmf_workers_item_count = count;
    gdmf_workers_chunk = chunk;
    atomic_store(&gdmf_workers_next, 0);
    gdmf_workers_participants = (threads < gdmf_workers_count + 1) ? threads : gdmf_workers_count + 1;
    gdmf_workers_pending = gdmf_workers_count;
    gdmf_workers_generation++;
    gdmf_workers_wake_start();
    gdmf_workers_unlock();

    gdmf_workers_work();

    gdmf_workers_lock();
    while (gdmf_workers_pending > 0) {
        gdmf_workers_wait_done();
    }
    gdmf_workers_busy = false;
    gdmf_workers_unlock();
}

void gdmf_workers_set_limit(int threads) {
    gdmf_workers_lock();
    gdmf_workers_limit = threads < 0 ? 0 : threads;
    gdmf_workers_unlock();
}

int gdmf_workers_threads(void) {
    gdmf_workers_lock();
    int threads = gdmf_workers_wanted_threads();
    if (gdmf_workers_started && threads > gdmf_workers_count + 1) {
        threads = gdmf_workers_count + 1;
    }
    gdmf_workers_unlock();
    return threads;
}

void gdmf_workers_shutdown(void) {
    gdmf_workers_lock();
    if (gdmf_workers_in_job || gdmf_workers_busy) {
        gdmf_workers_unlock();
        fprintf(stderr, "[GDMF] Worker shutdown requires completed jobs\n");
        return;
    }
    if (!gdmf_workers_started) {
        gdmf_workers_unlock();
        return;
    }
    gdmf_workers_quit = true;
    gdmf_workers_wake_start();
    gdmf_workers_unlock();

    for (int worker_index = 0; worker_index < gdmf_workers_count; worker_index++) {
        gdmf_workers_thread_join(gdmf_workers_handles[worker_index]);
    }

    gdmf_workers_lock();
    gdmf_workers_count = 0;
    gdmf_workers_started = false;
    gdmf_workers_quit = false;
    gdmf_workers_unlock();
}
