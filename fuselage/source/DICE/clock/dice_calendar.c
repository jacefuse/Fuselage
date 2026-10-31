// Calendar conversion. SEE: (DICE.md, Calendar time)
#if defined(__linux__)
#define _POSIX_C_SOURCE 200809L
#endif
#include "../dice.h"
#include <time.h>
#if defined(_WIN32)
#include <windows.h>
#endif

static int64_t dice_days_from_civil(int64_t year, unsigned month, unsigned day) {
    year -= (month <= 2);
    const int64_t era = (year >= 0 ? year : year - 399) / 400;
    const unsigned year_of_era = (unsigned)(year - era * 400);
    const unsigned day_of_year = (153u * (month + (month > 2 ? 0u : 12u) - 3u) + 2u) / 5u + day - 1u;
    const unsigned day_of_era = year_of_era * 365u + year_of_era / 4u - year_of_era / 100u + day_of_year;

    return era * 146097 + (int64_t)day_of_era - 719468;
}

typedef struct {
    uint64_t utc_seconds;
    uint32_t milliseconds;
    uint32_t year, month, day;
    uint32_t hour, minute, second;
    uint32_t is_dst;
} dice_calendar_reading;

#if defined(_WIN32)

static void dice_read_raw_time(dice_calendar_reading* reading) {

    FILETIME file_time;
    GetSystemTimePreciseAsFileTime(&file_time);
    ULARGE_INTEGER file_ticks;
    file_ticks.LowPart = file_time.dwLowDateTime;
    file_ticks.HighPart = file_time.dwHighDateTime;
    const unsigned long long unix_ticks = file_ticks.QuadPart - 116444736000000000ull;
    reading->utc_seconds = unix_ticks / 10000000ull;
    reading->milliseconds = (uint32_t)((unix_ticks / 10000ull) % 1000ull);

    SYSTEMTIME local_time;
    GetLocalTime(&local_time);
    reading->year = local_time.wYear;
    reading->month = local_time.wMonth;
    reading->day = local_time.wDay;
    reading->hour = local_time.wHour;
    reading->minute = local_time.wMinute;
    reading->second = local_time.wSecond;

    TIME_ZONE_INFORMATION zone_info;
    reading->is_dst = (GetTimeZoneInformation(&zone_info) == TIME_ZONE_ID_DAYLIGHT) ? 1u : 0u;

    return;
}

#else

static void dice_read_raw_time(dice_calendar_reading* reading) {
    struct timespec sample;
    clock_gettime(CLOCK_REALTIME, &sample);
    reading->utc_seconds = (uint64_t)sample.tv_sec;
    reading->milliseconds = (uint32_t)(sample.tv_nsec / 1000000L);

    time_t utc_time = sample.tv_sec;
    struct tm local_time;
    localtime_r(&utc_time, &local_time);
    reading->year = (uint32_t)(local_time.tm_year + 1900);
    reading->month = (uint32_t)(local_time.tm_mon + 1);
    reading->day = (uint32_t)local_time.tm_mday;
    reading->hour = (uint32_t)local_time.tm_hour;
    reading->minute = (uint32_t)local_time.tm_min;
    reading->second = (uint32_t)local_time.tm_sec;
    reading->is_dst = (local_time.tm_isdst > 0) ? 1u : 0u;

    return;
}

#endif

void DICE_GetRealTime(DICE_DateTime* out) {
    if (!out) {
        return;
    }

    dice_calendar_reading reading;
    dice_read_raw_time(&reading);

    const int64_t days = dice_days_from_civil((int64_t)reading.year, reading.month, reading.day);
    const int64_t local_seconds =
        days * 86400 + (int64_t)reading.hour * 3600 + (int64_t)reading.minute * 60 + (int64_t)reading.second;
    const int64_t offset_seconds = local_seconds - (int64_t)reading.utc_seconds;

    int64_t weekday = (days + 4) % 7;
    if (weekday < 0) {
        weekday += 7;
    }

    out->utcSeconds = reading.utc_seconds;
    out->milliseconds = reading.milliseconds;

    out->utcOffsetMinutes = (int32_t)((offset_seconds + (offset_seconds >= 0 ? 30 : -30)) / 60);
    out->isDst = reading.is_dst;
    out->year = reading.year;
    out->month = reading.month;
    out->day = reading.day;
    out->weekday = (uint32_t)weekday;
    out->hour = reading.hour;
    out->minute = reading.minute;
    out->second = reading.second;
    out->yearDay = (uint32_t)(days - dice_days_from_civil((int64_t)reading.year, 1u, 1u) + 1);
    out->_pad = 0u;

    return;
}
