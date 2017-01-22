#pragma once
#include <time.h>

#define START_TIMER(clock, start_timespec) clock_gettime(clock, &start_timespec);
#define END_TIMER(clock, end_timespec) clock_gettime(clock, &end_timespec);

#define TIME(clock, start_timespec, end_timespec, ...) \
    START_TIMER(clock, start_timespec) \
    __VA_ARGS__; \
    END_TIMER(clock, end_timespec)

// Uses process cpu time and the start and end variables, so not useful outside of this really...
#define PTIME(...) TIME(CLOCK_PROCESS_CPUTIME_ID, start, end, __VA_ARGS__)

/**
 * Calculates the delay between start and end in seconds.
 *
 * @param start The start time.
 * @param end   The end time.
 * @return The delay between start and end.
 */
double get_delay(struct timespec start, struct timespec end);