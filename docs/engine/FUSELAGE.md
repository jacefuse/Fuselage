# Fuselage native orchestration

## Lifecycle

Include `fuselage.h` for native engine controls. Register a callback with
`FuselageSetTickCallback`, then call `fuselage()` repeatedly on the main/render
thread. It initializes the engine on its first call. A true result means another
iteration is needed; false means shutdown has completed. Read
`FuselageGetExitCode()` afterwards: 0 is normal completion, 1 is a root startup or
runtime failure, and 5 means save publication failed. Launcher verdict codes 3
and 4 remain separate and must also be checked by the entry point.

The root owns GDMF, CAKE, SHARP and DICE for one process lifetime. Do not initialize
competing instances or reserve engine timer handles 0, 1 or 2 beforehand. Restart
after shutdown is not supported. Independent systems remain usable without the
root; their own headers describe their standalone lifecycle.

Startup creates resources and stopped timers before enabling the project callback.
A failed step enters shutdown and releases completed owners. An unavailable sound
device is allowed; SHARP still receives its cleanup call. Shutdown stops and joins
DICE workers before releasing save mappings, audio, input or graphics. GDMF owns
its graphics-layer teardown and waits for its renderer before releasing those
resources. If an owner cannot stop safely, the loop retains its dependencies and
returns true so shutdown can be retried. It does not promise a bounded recovery.

A clean shutdown attempts to publish SRAM. A failed startup or guest fault abandons
uncommitted changes. Save failure produces a nonzero exit instead of being silently
ignored. Guest memory is detached before the save mapping is closed. The launcher
must keep its ROAP bytes alive until this teardown completes.

## Configuration

Set title, window and canvas dimensions, aspect ratio, window icon, simulation
rate, audio-pool size and banner preference before the first loop call. Later
calls to these setters have no effect. The title is borrowed: keep its storage
valid through window creation. The icon setter copies its pixel data.

Window and canvas dimensions are separate sets. The canvas is the logical drawing
area; resizing a window scales that area. The resolution/aspect query functions
accept a null pointer for an unwanted output. They do not provide an atomic
snapshot of mutable backend state across threads.

The declared simulation rate defaults to 60 Hz and stays the value returned by
`FuselageGetSimRate`, even when display pacing adjusts the timer slightly. Invalid
nonpositive or nonfinite rates are ignored. An otherwise positive rate that DICE
cannot represent causes checked startup failure.

`FuselageSetTargetFPS` records a finite nonnegative cap; zero means uncapped.
Before startup, this value is used automatically. After startup, send
`FUSELAGE_APPLY_TARGET_FPS` to replace the timer. Failure to create, start or release
the replacement timer stops the root with exit code 1.

## Rendering and timing

Simulation callbacks run on a DICE worker. The root serializes them with graphics
preparation using a shared state mutex. Image acquisition and submission happen
outside that mutex. A simulation tick need not change drawing state for a frame
to be submitted. The callback must return so shutdown can join its worker.

`FuselageWaitFrame` waits for an independent auto-reset event, initially clocked
at 60 Hz. Display pacing may retune that clock with the simulation timer. It is
not an acknowledgement that a frame reached the display, and multiple signals
can coalesce. Calling it from the project callback is supported. Do not leave
external threads waiting during root shutdown; join those callers first.

Render caps use another event. While waiting, the render thread pumps window
messages and engine signals at approximately 10 ms intervals so a low cap does
not postpone quit handling until the next frame. The cap is a scheduling target,
not a guarantee of display timing. See FUSELAGE_PACING.md for the controller.

## Controls and threads

`FuselageSignal` queues quit, windowed, borderless, fullscreen-exclusive or cap
application requests. Repeated requests of one kind coalesce; these are flags,
not an ordered command queue. Quit takes priority. Display-mode requests are
applied on the render thread.

The tick-callback pointer, target-FPS value and focus-policy value use atomic
storage. `FuselageSetInputRequiresFocus` applies the desired policy on the next
simulation tick: it silences CAKE while focus is required and absent, then resumes
CAKE when focus or policy permits input. An explicit CAKE silence remains until
one of those policy/focus transitions occurs.

Other runtime controls forward to their systems. Their threading restrictions
still apply, including render-thread VSync changes. Atomic root controls do not
make all graphics/input queries safe from arbitrary threads.

## Windows DPI

Before creating a window, the root requests per-monitor DPI awareness version 2
when Windows supplies that API. This preserves physical-pixel window coordinates
on scaled displays. The request is optional and may be refused if another owner
already established process DPI awareness. The root has no equivalent setup call
on macOS or Linux; their window backends define the coordinate contract.
