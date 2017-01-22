#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <mqueue.h>
#include <semaphore.h>

#include <signal.h>
#include <ruby/missing.h>

#include "common.h"
#include "../timing.h"

// Names for the semaphore, shared memory, and message queue, respectively
#define COMP_8005_ASSN1_SEM "/comp_8005_assn1_sem"
#define COMP_8005_ASSN1_SHM "/comp_8005_assn1_shared_mem"
#define COMP_8005_ASSN1_MQ "/comp_8005_assn1_shared_mq"

#define SEM_NAME_BUFSIZE 256

typedef struct {
    size_t job_id;
    size_t prime;
} job_result_msg;

static void do_sieve(size_t job_id, size_t slice_start, size_t slice_size, int shared_mem, mqd_t msg_queue) {
    char* composites = calloc(slice_size, sizeof(char));

    // Timing info
    struct timespec start, end;
    double ms_waiting = 0.0;
    double ms_ipc = 0.0;
    double ms_working = 0.0;

    // Set up IPC
    char sem_name[SEM_NAME_BUFSIZE];
    snprintf(sem_name, SEM_NAME_BUFSIZE, "%s%lu", COMP_8005_ASSN1_SEM, job_id);
    sem_t* sem = sem_open(sem_name, O_RDWR);

    // Map the shared memory for reading the next prime
    size_t* unmarked = mmap(NULL, sizeof(size_t), PROT_READ, MAP_SHARED, shared_mem, 0);

    size_t new_min = 0;
    do {
        PTIME(sem_wait(sem))
        ms_waiting += get_delay(start, end);

        PTIME(new_min = strike_multiples(composites, slice_size, slice_start, *unmarked))
        ms_working += get_delay(start, end);

        job_result_msg msg;
        msg.job_id = job_id;
        msg.prime = new_min;

        PTIME(int result = mq_send(msg_queue, (char*)&msg, sizeof(msg), 0))
        ms_ipc += get_delay(start, end);

        if (result == -1) {
            perror("Error sending message");
            exit(EXIT_FAILURE);
        }
    } while(new_min != (size_t)-1);

    // Create a file and print all of the primes to it
    PTIME
    (
        char buf[32];
        snprintf(buf, 32, "proc%lu", job_id);
        FILE* out = fopen(buf, "w");

        printf("Writing results to file %s\n", buf);
        for (size_t i = 0; i < slice_size; ++i) {
            if (!composites[i]) {
                fprintf(out, "%lu\n", i + slice_start);
            }
        }
        fclose(out);
    )

    ms_working += get_delay(start, end);
    snprintf(buf, 32, "proc%lu.time", job_id);
    FILE* timing = fopen(buf, "w");
    fprintf(timing, "Milliseconds waiting for semaphore: %.4lf\n", ms_waiting);
    fprintf(timing, "Milliseconds working: %.4lf\n", ms_working);
    fprintf(timing, "Milliseconds sending messages: %.4lf\n", ms_ipc);
    fclose(timing);

    sem_close(sem);
    munmap(unmarked, sizeof(size_t));
    free(composites);
    exit(EXIT_SUCCESS);
}

void concurrent_sieve_process(size_t limit, size_t num_jobs) {

    // Ignore SIGCHLD to automatically clean up child processes
    signal(SIGCHLD, SIG_IGN);

    // Create a message queue to send back the minimum prime from each process
    struct mq_attr attr = {0};
    attr.mq_flags = 0;
    attr.mq_maxmsg = num_jobs + 5;
    attr.mq_msgsize = sizeof(job_result_msg);
    attr.mq_curmsgs = 0;

    mqd_t msg_queue = mq_open(COMP_8005_ASSN1_MQ, O_CREAT | O_RDWR, 0666, &attr);
    if (msg_queue == -1) {
        perror("Error opening message queue");
        exit(EXIT_FAILURE);
    }

    // Initialise shared memory to hold the new minimum prime
    int shared_mem = shm_open(COMP_8005_ASSN1_SHM, O_CREAT | O_RDWR, 0666);
    ftruncate(shared_mem, sizeof(size_t));
    size_t* next_prime;

    // Create a semaphore so that the other processes can wait for the new unmarked prime
    sem_t** semaphores = malloc(sizeof(sem_t*) * num_jobs);

    size_t start = 3;
    size_t extra;
    size_t slice_size;

    get_slices(num_jobs, limit, &slice_size, &extra);

    for (size_t i = 0; i < num_jobs; ++i) {
        char sem_name[SEM_NAME_BUFSIZE];
        snprintf(sem_name, SEM_NAME_BUFSIZE, "%s%lu", COMP_8005_ASSN1_SEM, i);

        semaphores[i] = sem_open(sem_name, O_CREAT | O_RDWR, 0666, 0);
        if (!fork()) {
            size_t slice_start = start + (i * slice_size);
            size_t cur_slice_size = slice_size + (i == num_jobs - 1 ? extra : 0); // assign any extra to the last job
            do_sieve(i, slice_start, cur_slice_size, shared_mem, msg_queue);
        }
    }

    next_prime = mmap(NULL, sizeof(size_t), PROT_READ | PROT_WRITE, MAP_SHARED, shared_mem, 0);
    if (next_prime == MAP_FAILED) {
        perror("Failed to map memory:");
        exit(EXIT_FAILURE);
    }

    size_t remaining_jobs = num_jobs;
    size_t new_min = 2;
    while (remaining_jobs) {
        *next_prime = new_min;

        // Wake up the job processes
        for(size_t i = 0; i < num_jobs; ++i) {
            if (semaphores[i]) {
                int result = sem_post(semaphores[i]);
                if (result == -1) {
                    perror("Error while posting to semaphore");
                    exit(1);
                }
            }
        }

        new_min = (size_t)-1;
        size_t msg_count = 0;
        size_t finished_jobs = 0;

        // Read the result messages from each job process
        while(msg_count < remaining_jobs) {

            // Apparently the message buffer needs to be bigger than max message size, so copy it into a buffer
            // that's one byte bigger
            char buf[sizeof(job_result_msg) + 1];
            job_result_msg job_result;

            ssize_t result = mq_receive(msg_queue, buf, sizeof(job_result), NULL);
            if (result == -1) {
                perror("Error while receiving message");
                exit(1);
            }
            memcpy(&job_result, buf, sizeof(job_result_msg));

            ++msg_count;
            if (job_result.prime == (size_t)-1) {

                // This job has finished - close its semaphore
                char sem_name[SEM_NAME_BUFSIZE];
                snprintf(sem_name, SEM_NAME_BUFSIZE, "%s%lu", COMP_8005_ASSN1_SEM, job_result.job_id);

                sem_close(semaphores[job_result.job_id]);
                sem_unlink(sem_name);
                semaphores[job_result.job_id] = NULL;

                ++finished_jobs;

            } else if (job_result.prime < new_min) {
                new_min = job_result.prime;
            }
        }

        remaining_jobs -= finished_jobs;
    }

    // Clean up
    munmap(next_prime, 0);
    shm_unlink(COMP_8005_ASSN1_SHM);

    mq_close(msg_queue);
    mq_unlink(COMP_8005_ASSN1_MQ);

    free(semaphores);
}