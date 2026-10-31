# Internal synchronization

## Synchronization contract

`BASE/fuselage_sync.h` supplies header-only mutex and auto-reset event helpers for
engine systems. Initialize each object before sharing it. Stop and join its
users before destruction; no thread may still hold, wait on, or signal it.
Do not copy an initialized object or initialize it again without destroying it.

The original helpers return `void` and do not report native API failures.
Checked creation variants are described below. Ordinary waits still have no
returned status; their contract assumes the underlying operations succeed.

## Mutex behavior

Use balanced lock/unlock calls from the owning thread, without recursive
locking. Windows uses `CRITICAL_SECTION`, which supports recursion, but the
Linux/macOS implementation does not. Windows has no FIFO handoff guarantee
in this interface.

Linux and macOS use a ticket counter protected by a pthread mutex and condition
variable. Threads enter in ticket order. Unlock advances the serving ticket and
broadcasts so the matching waiter can proceed. Every awakened thread rechecks
its ticket, including after a spurious wakeup.

This prevents a thread from immediately reacquiring ahead of an already queued
waiter, an important constraint when a simulation tick holds the state lock for
an application-dependent amount of work. Ticket order does not bound execution
time or guarantee that the operating system schedules a waiting thread promptly.
An abandoned ticket blocks subsequent tickets; callers must not cancel a waiter.

Unsigned counters wrap by C's unsigned arithmetic rules. Correct ordering requires
that outstanding tickets never span the entire counter range; wrap alone does
not require resetting an idle lock.

## Auto-reset events

An event starts unsignaled. Setting it records one pending signal and allows one
wait to consume that signal. Setting an already signaled event does not accumulate
another signal. Setting it before any thread waits allows a later wait to return
immediately. A successful wait consumes the signal and resets the event.

Windows uses an auto-reset event handle. Linux and macOS protect a Boolean flag
with a pthread mutex and condition variable. Neither implementation promises
which waiter receives the signal. Use a separate counter or queue when every
notification must be counted.

## Checked creation

The root uses `fuselage_mutex_init_checked` and `fuselage_event_init_checked`.
They return false when native creation fails. POSIX creation rolls back a mutex
if its accompanying condition variable cannot be created. A failed object must
not be published, used or destroyed as an initialized object. Existing unchecked
helpers remain available to earlier consumers; their callers have not all been
converted. Lock, unlock, signal and ordinary wait helpers still do not report
native operation failures. Events do not provide broadcast cancellation.
