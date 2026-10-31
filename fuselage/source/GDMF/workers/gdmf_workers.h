#ifndef GDMF_WORKERS_H
#define GDMF_WORKERS_H

// Internal synchronous jobs. Callbacks read stable shared state and write only
// their own output ranges. SEE: (GDMF_WORKERS.md, Job contract)
typedef void (*gdmf_work_callback)(int begin, int end, void* context);

// Each item in [0,count) is assigned once. Nonpositive count/NULL callback do nothing.
// Nested or concurrent jobs execute inline; the caller participates in pooled jobs.
void gdmf_workers_run(int count, int chunk, gdmf_work_callback callback, void* context);
// 0 or negative selects logical CPU count; 1 selects inline work. Maximum 64 threads.
void gdmf_workers_set_limit(int threads);
int gdmf_workers_threads(void);
// Requires no active jobs/callers. Joins workers; may be called before startup or reused.
void gdmf_workers_shutdown(void);

#endif
