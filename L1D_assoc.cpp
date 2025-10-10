#include <cstdio>
#include <vector>
#include <malloc.h>
#include <x86intrin.h>
#include <algorithm>
#include <random>
#include "cacheutil.h"
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "Usage: %s <cache_size_KB> <max_assoc_guess>\n", argv[0]);
        return 1;
    }
    size_t cache_kb = std::stoul(argv[1]);
    size_t cache_bytes = cache_kb << 10;
    size_t max_assoc = std::stoul(argv[2]);

    constexpr uint64_t ITER = 10'000'000;
    double last =0;
    //printf("Assoc,Cycles/(access),Stride\n");
    for (size_t assoc_guess = 1; assoc_guess <= max_assoc; ++assoc_guess) {
        size_t stride = cache_bytes / assoc_guess;

        size_t n = assoc_guess + 1;

        size_t alloc_bytes = stride * n + CACHE_LINE_SIZE;
        char* mem = (char*) ALLOC(alloc_bytes);
        if (!mem) {
            perror("malloc");
            return 1;
        }

        auto addr = reinterpret_cast<uintptr_t>(mem);
        uintptr_t aligned = (addr + 63) & ~uintptr_t(63);
        Node* base = reinterpret_cast<Node*>(aligned);

        std::vector<Node*> nodes(n);
        for (size_t i = 0; i < n; ++i) {
            nodes[i] = reinterpret_cast<Node*>(
                reinterpret_cast<char*>(base) + i * stride
            );
        }

        for (size_t i = 0; i < n; ++i) {
            nodes[i]->next = nodes[(i + 1) % n];
        }

        for (auto p : nodes) {
            _mm_clflush(p);
        }

        Node* p = nodes[0];
        uint64_t t0 = rdtscp();
        for (uint64_t it = 0; it < ITER; ++it) {
            p = p->next;
        }
        uint64_t t1 = rdtscp();

        double cyc = double(t1 - t0) / double(ITER);
        if ( assoc_guess > 1 && cyc/last > 1.6){
            std::cout << "L1_assoc " << assoc_guess;
            return 0;
        }
        last = cyc;

        FREE(mem);
    }
    return 0;
}
