#pragma once
#include <cstdint>
#include <x86intrin.h>
#include <pthread.h>
#include <sched.h>
#include <cstdlib>
#include <iostream>

#define CACHE_LINE_SIZE 64

// Use posix_memalign for cache-line alignment
#define ALLOC(size) [] (size_t sz) { void* p=nullptr; if (posix_memalign(&p, CACHE_LINE_SIZE, sz)!=0) p=nullptr; return p; }(size)
#define FREE(p)     free(p)

static inline uint64_t rdtscp() {
    unsigned aux;
    return __rdtscp(&aux);
}

struct alignas(CACHE_LINE_SIZE) Node {
    Node* next;
    char pad[CACHE_LINE_SIZE - sizeof(Node*)];
};

void set_cpu_affinity(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    int result = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (result != 0) {
        std::cerr << "pthread_setaffinity_np failed (error " << result << ")\n";
    }
}
