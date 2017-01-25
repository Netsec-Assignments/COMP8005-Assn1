#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>

#include "sieve/common.h"
#include "sieve/process.h"
#include "sieve/thread.h"
#include "timing.h"

const size_t DEFAULT_LIMIT = 100000;
const size_t DEFAULT_NUM_JOBS = 5;

void print_help(char const* prog_name) {
    printf("calculates the primes below a limit using the sieve of Eratsothenes.\n");
    printf("usage: %s [-t | -p] [-j jobs] [-l limit]\n", prog_name);
    printf("\t-t: perform the sieve using threads. Cannot be used with the -p option.\n");
    printf("\t-p: perform the sieve using processes. Cannot be used with the -t option.\n");
    printf("\t-j: optional argument to specify the number of jobs (threads or processes).\n");
    printf("\t    Default is %lu. If -l is given, the number of jobs must be <= limit - 3.\n", DEFAULT_NUM_JOBS);
    printf("\t-l: optional argument to specify the limit for the sieve. Must be >= 10.\n");
    printf("\t    Default is %lu.\n", DEFAULT_LIMIT);
    printf("\t-h: print this help message and exit.\n");
}

int main(int argc, char** argv) {
    int thread_flag = 0;
    int process_flag = 0;

    size_t limit = DEFAULT_LIMIT;
    size_t num_jobs = DEFAULT_NUM_JOBS;

    opterr = 0;
    int c;
    while((c = getopt(argc, argv, "tphj:l:")) != -1) {
        switch(c) {
            case 't':
                thread_flag = 1;
                if(process_flag) {
                    fprintf(stderr, "-t and -p flags are mutually exclusive. See %s -h for help.\n", argv[0]);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'p':
                process_flag = 1;
                if (thread_flag) {
                    fprintf(stderr, "-t and -p flags are mutually exclusive. See %s -h for help.\n", argv[0]);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'h':
                print_help(argv[0]);
                exit(EXIT_SUCCESS);
                break;
            case 'j':
                if(sscanf(optarg, "%lu", &num_jobs) != 1) {
                    fprintf(stderr, "Invalid argument %s for -j. See %s -h for help.\n", optarg, argv[0]);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'l':
                if(sscanf(optarg, "%lu", &limit) != 1) {
                    fprintf(stderr, "Invalid argument %s for -l. See %s -h for help.\n", optarg, argv[0]);
                    exit(EXIT_FAILURE);
                }
                break;
            case '?':
                if (optopt == 'j') {
                    fprintf(stderr, "No argument given for -j. See %s -h for help.\n", argv[0]);
                    exit(EXIT_FAILURE);
                } else if (optopt == 'l') {
                    fprintf(stderr, "No argument given for -l. See %s -h for help.\n", argv[0]);
                    exit(EXIT_FAILURE);
                } else {
                    fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
                    exit(EXIT_FAILURE);
                }
            default:
                fprintf(stderr, "You broke getopt()...\n");
                exit(EXIT_FAILURE);
                break;
        }
    }

    if(limit < 10) {
        fprintf(stderr, "Limit must be >= 10.\n");
        exit(EXIT_FAILURE);
    } else if (num_jobs > limit - 3) {
        fprintf(stderr, "Number of jobs must be <= limit -3.\n");
        exit(EXIT_FAILURE);
    }

    struct timespec start, end;
    if(thread_flag) {
        CTIME(concurrent_sieve_thread(limit, num_jobs))
    } else if (process_flag) {
        CTIME(concurrent_sieve_process(limit, num_jobs))
    } else {
        fprintf(stderr, "Must specify threads or processes; see %s -h for help.\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    double total_ms = get_delay(start, end);
    printf("Total time: %.4fms\n", total_ms);

    return EXIT_SUCCESS;
}
