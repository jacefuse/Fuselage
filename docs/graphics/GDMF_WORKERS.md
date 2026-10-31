# GDMF workers

The internal worker pool runs independent ranges of CPU work. Sprites collision
batches use it when their estimated workload is large enough to justify waking
workers. It is not a public application task scheduler.

## Job contract

`gdmf_workers_run` assigns each item in `[0, count)` once and waits for the job
to finish before returning. A callback receives a half-open range and the
caller's context pointer. It may read shared state held stable by its caller
and write its own output range. The pool does not acquire the engine state lock.
Keep the context and outputs alive until the call returns.

The caller participates in a pooled job. Chunks are assigned dynamically, so
callback order and executing thread are unspecified. A nonpositive count or
NULL callback does nothing; a nonpositive chunk size becomes one.

Only one job uses the pool at a time. A nested call or a call made while another
pooled job is active executes inline on its own calling thread. That fallback
does not synchronize conflicting application data or make Sprite APIs reentrant.
Inline callbacks receive the whole range in one call.

## Limits and lifecycle

`gdmf_workers_set_limit(1)` selects inline execution. Zero or a negative value
selects the logical CPU count. The maximum is 64 participating threads, including
the caller. The first pooled job creates up to one worker per logical CPU after
the caller, bounded by that maximum. A lower limit leaves excess workers parked;
it does not reduce the number initially created.

`gdmf_workers_threads` reports the configured count, reduced to available workers
after startup when thread creation failed. It is not a count of threads currently
executing a callback. Thread-limit access is protected by the pool lock.

Call `gdmf_workers_shutdown` only after excluding new callers and completing all
jobs. It joins existing workers and permits later startup. It can be called
before any worker exists. Calls made from a callback or during a pooled job are
refused with a diagnostic; the void interface does not provide a success flag.
This guard does not replace lifecycle coordination for concurrent inline callers.

## Platforms and builds

Windows uses native threads, SRW locks and condition variables. Linux and macOS
use pthreads. These implementations share the job-distribution logic.

The thread-local marker prevents nested jobs from attempting pooled execution.
With the tested MinGW Clang toolchain, default emulated thread-local storage
imports winpthreads indirectly. Compiling with `-fno-emulated-tls` uses native
Windows TLS instead. The standalone collision target applies this option only
to Windows Clang builds outside the MSVC toolchain. Carry that choice into the
production build and inspect the resulting executable's imports.

The standalone Windows worker/collision executable has been tested without a
winpthreads import. This does not establish the dependency list of a complete
engine, launcher or application build.
