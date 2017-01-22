#pragma once

typedef int (*memory_alloc)(int is_master, size_t size, void** out);
typedef void (*memory_free)(int is_master, size_t size, void* mem);

typedef struct {
    memory_alloc alloc;
    memory_free free;
} allocator_t;

extern const allocator_t* process_allocator;
extern const allocator_t* thread_allocator;
