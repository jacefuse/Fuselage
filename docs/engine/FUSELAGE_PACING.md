# Display pacing

## Controller

The root measures the intervals between successful graphics-preparation samples.
These are useful observations of rendering cadence; they do not prove when a
frame became visible. With VSync, driver/compositor behavior still affects them.

When the measured cadence is within 1% of the declared simulation rate, or an
integer multiple in either direction up to eight, the controller can retune the
simulation timer and the `FuselageWaitFrame` timer together. The declared rate
reported to applications stays unchanged. A tick-counted simulation can therefore
run slightly faster or slower; elapsed-time clocks continue to report real time.

The first estimate is the median of 64 positive intervals. A 256-sample history
then refines it; at least 60 good intervals are required before locking. A ratio
error above 1.5% unlocks, restoring the declared period. Gaps above half a second,
repeated cadence errors or backwards timestamps reset measurement. Identical
timestamps contribute no interval.

While locked, small simulation-timer phase adjustments aim to leave about 2 ms
between a completed tick and the graphics sample. A correction is limited to 5%
of the current period. It shifts timer targets; it does not synthesize extra
callbacks or guarantee one tick for every visible frame.

The implementation is divided into controller/sample handling, cadence estimation
and timer steering, and trace reporting. All operations use the root state lock,
except initialization before callbacks are admitted and shutdown after they join.

## Timer failures

Timer retunes return a status. If applying a new period to either owned timer
fails, the controller attempts to restore both declared periods and disables
steering. Because that restoration can also fail, the status reports period zero
rather than claiming a known period. A failed phase adjustment unlocks and
requests the declared period. Coordinated timer changes are not transactional.

Shutdown clears controller state. The private status query returns false when
unlocked or inactive; optional outputs are estimated refresh rate, signed ratio
and applied/declared period in microseconds. A positive ratio means multiple
simulation ticks per measured refresh; a negative ratio means multiple measured
refreshes per simulation tick. Zero means no locked ratio. Refresh and period
outputs are zero when inactive; the period is also zero after an unresolved
period-retune failure.

## Diagnostics

Set `FUSELAGE_PACING=off` to keep measuring without steering. Set
`FUSELAGE_PACING_TRACE=1` for periodic stderr reports and lock/unlock messages;
`FUSELAGE_PACING_TRACE=2` also reports each locked sample. These settings are read
at initialization. Trace measurements include callback counts, state-lock wait,
preparation/submission durations and sample lag. They are diagnostic observations,
not a display-presentation counter.
