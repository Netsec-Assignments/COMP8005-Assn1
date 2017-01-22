#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>           /* For O_* constants */

#include "memory.h"

#define SHARED_MEM_FILE "/composites_array"

int process_mem_alloc(int is_master, size_t size, void** out) {
    int fd = shm_open(SHARED_MEM_FILE, O_CREAT|O_RDWR, 0666);
    if (fd == -1) {
        perror("process_mem_alloc:");
        return -1;
    }

    if (is_master) {
        int result = ftruncate(fd, (off_t)size);
        if (result == -1) {
            perror("process_mem_alloc:");
            shm_unlink(SHARED_MEM_FILE);
            return -1;
        }
    }

    void* mapped = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped == MAP_FAILED) {
        perror("process_mem_alloc:");
        shm_unlink(SHARED_MEM_FILE);
        return -1;
    }

    *out = mapped;
    return 0;
}

void process_mem_free(int is_master, size_t size, void* mem) {
    if (is_master) {
        shm_unlink(SHARED_MEM_FILE);
    }
    munmap(mem, size);
}

int thread_mem_alloc(int is_master, size_t size, void** out) {
    static void* mem = NULL; // OK, this is a hack

    // This, even more so: we're assuming that the master thread calls this first
    if (is_master) {
        mem = calloc(1, size);
        if (!mem) {
            perror("thread_mem_alloc:");
            return -1;
        }

        *out = mem;
    }
    *out = mem;
    return 0;
}

void thread_mem_free(int is_master, size_t size, void* mem) {
    if (is_master) {
        free(mem);
    }
}

static allocator_t p_allocator = {process_mem_alloc, process_mem_free};
static allocator_t t_allocator = {thread_mem_alloc, thread_mem_free};

const allocator_t* process_allocator = &p_allocator;
const allocator_t* thread_allocator = &t_allocator;
