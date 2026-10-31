/* Shared live/page clock derivation. SEE: (MISL_HOST_SERVICES.md, Dice) */
#include "../runtime/misl_host_pages_internal.h"
#include "../../../DICE/dice.h"

void fuselage_guest_fill_clock(MislClock* output) {
    if (!output) {
        return;
    }
    DICE_DateTime calendar;
    DICE_GetRealTime(&calendar);
    output->utcSeconds = calendar.utcSeconds;
    output->milliseconds = calendar.milliseconds;
    output->utcOffsetMinutes = calendar.utcOffsetMinutes;
    output->isDst = calendar.isDst;
    output->year = calendar.year;
    output->month = calendar.month;
    output->day = calendar.day;
    output->weekday = calendar.weekday;
    output->hour = calendar.hour;
    output->minute = calendar.minute;
    output->second = calendar.second;
    output->yearDay = calendar.yearDay;
    output->_pad = 0;
}
