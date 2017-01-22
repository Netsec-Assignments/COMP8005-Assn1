#pragma once
#include <stddef.h>

/**
 * Strikes out all multiples of unmarked in the composites array.
 *
 * @param composites The list of bools indicating whether each number is composite or prime.
 * @param len        The length of the composites list.
 * @param start      The starting number for the composites list.
 * @param unmarked   The unmarked number whose multiples will be struck.
 */
size_t strike_multiples(char* composites, size_t len, size_t start, size_t unmarked);

/**
 * Runs the sieve of Eratosthenes entirely on the current thread and prints the results to stdout (for now).
 * @param limit The limit
 */
void serial_sieve(size_t limit);

/**
 * Gets the size of number ranges assigned to each job and the extra numbers for the last job (if any).
 * @param num_jobs    The number of jobs that the range is to be divided between. Must be > limit - 2.
 * @param limit       The number below which all primes will be calculated.
 * @param slice_size  The number of numbers that each job will process.
 * @param extra       The extra numbers processed by the last job (may be zero).
 */
void get_slices(size_t num_jobs, size_t limit, size_t* slice_size, size_t* extra);