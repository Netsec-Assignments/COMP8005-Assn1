#include "timing.h"

double get_delay(struct timespec start, struct timespec end) {
    return ((end.tv_sec - start.tv_sec) * 1000.f) + ((end.tv_nsec - start.tv_nsec) / 1000000.f);
}