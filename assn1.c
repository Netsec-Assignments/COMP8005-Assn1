#include <stdio.h>

#include "sieve/common.h"
#include "sieve/process.h"

const size_t LIMIT = 100000;

int main(int argc, char** argv) {
//    concurrent_sieve_process(LIMIT, 2);
    serial_sieve(LIMIT);

    return 0;
}
