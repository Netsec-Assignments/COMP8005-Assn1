#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "common.h"

size_t strike_multiples(char* composites, size_t len, size_t start, size_t unmarked) {
    size_t min_unmarked = (size_t)-1;
    size_t mult_start = unmarked + 1; // The first number that's a candidate for crossing out

    // Any numbers that can be evenly divided by the current unmarked number are not prime
    for (size_t i = mult_start > start ? mult_start - start : 0; i < len; ++i) {
        size_t num = i + start;
        if (num % unmarked == 0) {
            composites[i] = 1;
        } else if (min_unmarked == (size_t)-1) {
            min_unmarked = num;
        }
    }

    return min_unmarked;
}

void serial_sieve(size_t limit) {
    char* composites = calloc(1, limit - 1);
    size_t root = (size_t)sqrt(limit);

    /* Find the primes below the limit */
    for (size_t i = 2; i <= root;) {
        i = strike_multiples(composites + 2, limit - 3, 3, i);
    }

    for (size_t i = 0; i < limit - 1; ++i) {
        if(!composites[i]) {
            printf("%lu\n", i + 1);
        }
    }

    free(composites);
}

void get_slices(size_t num_jobs, size_t limit, size_t* slice_size, size_t* extra) {
    size_t end = limit - 1;

    size_t range = end - 2;
    *slice_size = range / num_jobs;
    *extra = range % num_jobs;
}