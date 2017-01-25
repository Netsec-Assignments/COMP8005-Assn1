#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include "thread.h"
#include "common.h"
#include "../timing.h"

typedef struct {
    // Synchronisation and "IPC" variables
    pthread_mutex_t cond_mutex;
    pthread_cond_t cond_var;
    atomic_size_t slave_minimum; // The variable that slaves will write the new minimum to
    atomic_size_t master_minimum; // Contains the new minimum written by the master; slaves wait for this to change

    // Info about remaining jobs
    atomic_size_t waiting_jobs; // Jobs that have finished work for this iteration
    atomic_size_t completed_jobs; // Jobs that finished all work in this iteration
    atomic_size_t filed_jobs; // Number of jobs that have written all info to file
} thread_sync_t;

typedef struct {
    size_t job_id;
    size_t slice_start;
    size_t slice_size;
    char* composites;

    thread_sync_t* sync;
} job_params_t;

static void* do_sieve(void* job_params) {
    job_params_t* params = (job_params_t*)job_params;

    size_t prev_min = 0;
    size_t job_min = 0;

    struct timespec start, end;
    double ms_ipc = 0.0;
    double ms_working = 0.0;

    while (1) {
        size_t new_min;

        CTIME
        (
            atomic_fetch_add(&params->sync->waiting_jobs, 1);

            if (job_min == (size_t)-1) {
                atomic_fetch_add(&params->sync->completed_jobs, 1);
                break;
            }

            pthread_mutex_lock(&params->sync->cond_mutex);
            while((new_min = atomic_load(&params->sync->master_minimum)) == prev_min) {
                pthread_cond_wait(&params->sync->cond_var, &params->sync->cond_mutex);
            }
            pthread_mutex_unlock(&params->sync->cond_mutex);
        )
        ms_ipc += get_delay(start, end);
        prev_min = new_min;


        CTIME(job_min = strike_multiples(params->composites, params->slice_size, params->slice_start, new_min))
        ms_working += get_delay(start, end);

        CTIME
        (
            if (job_min != (size_t)-1) {
                // Keep trying to write the job's minimum value to the slave minimum variable until either
                // * The write succeeds, or
                // * The new value is <= our minimum value
                size_t cur_slave_min = atomic_load(&params->sync->slave_minimum);
                while(1) {
                    if (cur_slave_min <= job_min) {
                        break;
                    } else {
                        if (atomic_compare_exchange_strong(&params->sync->slave_minimum, &cur_slave_min, job_min)) {
                            break;
                        }
                    }
                }
            }
        )
        ms_ipc += get_delay(start, end);
    }

    CTIME
    (
        char primes_filename[32];
        snprintf(primes_filename, 32, "thread%lu", params->job_id);
        FILE* primes = fopen(primes_filename, "w");
        for(size_t i = 0; i < params->slice_size; ++i) {
            if (!params->composites[i]) {
                fprintf(primes, "%lu\n", i + params->slice_start);
            }
        }
        fclose(primes);
    )
    ms_working += get_delay(start, end);

    char timing_filename[32];
    snprintf(timing_filename, 32, "thread%lu.time", params->job_id);
    FILE* timing = fopen(timing_filename, "a");
    fprintf(timing, "time doing IPC (ms): %.4lf\n", ms_ipc);
    fprintf(timing, "time working (ms): %.4lf\n", ms_working);
    fclose(timing);

    printf("Job %lu finished. Primes written to %s. Timing info written to %s.\n", params->job_id, primes_filename, timing_filename);

    free(job_params);
    atomic_fetch_add(&params->sync->filed_jobs, 1);
    return NULL;
}

void concurrent_sieve_thread(size_t limit, size_t num_jobs) {

    char* composites = calloc(limit - 3, 1);
    if (!composites) {
        perror("Error while allocating memory:");
        exit(EXIT_FAILURE);
    }

    thread_sync_t s;
    s.slave_minimum = 2;
    s.master_minimum = 0;
    s.waiting_jobs = 0;
    s.completed_jobs = 0;
    s.filed_jobs = 0;

    if (pthread_mutex_init(&s.cond_mutex, NULL) == -1) {
        perror("Error while initialising mutex:");
        free(composites);
        exit(EXIT_FAILURE);
    }

    if (pthread_cond_init(&s.cond_var, NULL) == -1) {
        perror("Error while initialising cond variable:");
        pthread_mutex_destroy(&s.cond_mutex);
        free(composites);
        exit(EXIT_FAILURE);
    }

    size_t start = 3;
    size_t extra;
    size_t slice_size;

    get_slices(num_jobs, limit, &slice_size, &extra);

    for (size_t i = 0; i < num_jobs; ++i) {
        size_t slice_start = start + i * slice_size;
        size_t cur_slice_size = slice_size + (i == num_jobs - 1 ? extra : 0);

        job_params_t* params = malloc(sizeof(job_params_t));
        if (!params) {
            perror("Error while allocating job parameters:");
            free(composites);
            pthread_mutex_destroy(&s.cond_mutex);
            pthread_cond_destroy(&s.cond_var);
            exit(EXIT_FAILURE);
        }

        params->job_id = i;
        params->slice_start = slice_start;
        params->slice_size = cur_slice_size;
        params->composites = composites + (i * slice_size);
        params->sync = &s;

        pthread_t thread;
        pthread_create(&thread, NULL, do_sieve, params);
        pthread_detach(thread);
    }

    size_t remaining_jobs = num_jobs;
    size_t new_min = 2;
    while (1) {
        // Wait for all threads to sleep
        while(atomic_load(&s.waiting_jobs) != remaining_jobs);

        remaining_jobs -= atomic_load(&s.completed_jobs);
        if (remaining_jobs == 0) {
            break;
        }

        atomic_store(&s.completed_jobs, 0);
        atomic_store(&s.waiting_jobs, 0);

        // Reset the slave minimum
        new_min = atomic_load(&s.slave_minimum);
        atomic_store(&s.slave_minimum, (size_t)-1);

        // Signal the threads. Grab the mutex just in case one of the threads happens to spuriously
        // wake up, read the new variable, and finish all its work between setting the new minimum
        // and signaling the condition var
        pthread_mutex_lock(&s.cond_mutex);
        atomic_store(&s.master_minimum, new_min);
        pthread_cond_broadcast(&s.cond_var);
        pthread_mutex_unlock(&s.cond_mutex);
    }

    // Wait for all threads to finish writing to file
    while(atomic_load(&s.filed_jobs) != num_jobs);

    // Clean up
    pthread_mutex_destroy(&s.cond_mutex);
    pthread_cond_destroy(&s.cond_var);

    free(composites);
}