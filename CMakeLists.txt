cmake_minimum_required(VERSION 2.8)
project("COMP 8005 Assn1")

set(SOURCES assn1.c
            memory.c sieve/common.c sieve/process.c sieve/process.h sieve/thread.c timing.c timing.h)

add_executable(assn1 ${SOURCES})
target_link_libraries(assn1 -lm -lpthread -lrt)
