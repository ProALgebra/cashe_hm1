#include <cstdio>
#include <cstdint>
#include <x86intrin.h>
#include <bits/stdc++.h>
#include <immintrin.h>
#include <x86intrin.h>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>
#include <random>
#include <malloc.h>
#include "cacheutil.h"
#include <iostream>
#include <cstdio>
#include <vector>
#include <malloc.h>
#include <x86intrin.h>
#include <algorithm>
#include <random>
#include "cacheutil.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <cstdio>
#include <bits/stdc++.h>
#include <x86intrin.h>
#include <cstdlib>
#include <thread>
#include <atomic>
#include <iostream>
#include <chrono>
#include <unistd.h>


size_t cache_kb = 30;
size_t max_assoc = 30;
const size_t   STEP_KB = 2;
const size_t   MAX_KB  = 256;
const uint64_t ITER    = 10'000'000;
const int RUNS = 10;


void cache_assoc(){
    constexpr uint64_t ITER = 10'000'000;
    double last =0;
    std::cout << "Cache size = " << cache_kb << " KB" << std::endl;
    size_t cache_bytes = cache_kb << 10;
    //printf("Assoc,Cycles/(access),Stride\n");
    int maxi = 0;
    last = 0;
    int index = 0;
    for (size_t assoc_guess = 1; assoc_guess <= max_assoc; ++assoc_guess) {
        size_t stride = cache_bytes ;

        size_t n = assoc_guess + 1;

        size_t alloc_bytes = stride * n  + CACHE_LINE_SIZE;
        char* mem = (char*) ALLOC(alloc_bytes);
        if (!mem) {
            perror("malloc");
            exit(1);
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

        Node* p = nodes[0];
        for (uint64_t it = 0; it < ITER; ++it) {
            p = p->next;
        }

        // iterating over linked list
        uint64_t t0 = rdtscp();
        for (uint64_t it = 0; it < ITER; ++it) {
            p = p->next;
        }
        uint64_t t1 = rdtscp();
        FILE* null_file = fopen("/dev/null", "w");
        if (null_file != NULL) {
            fprintf(null_file, "%p", (void*)p);
            fclose(null_file);
        }
        double cyc = double(t1 - t0) / double(ITER);
        printf("%4zu,%.2f,%6zu\n", assoc_guess, cyc, stride);
        if(assoc_guess > 1 &&  1.4 < cyc/last){
            maxi = cyc/last;
            index = assoc_guess;
            std::cout << "L1D assoc = " << index << std::endl;
            break;
        }

        last = cyc;

        FREE(mem);
    }
}

void cache_size(){
    std::mt19937_64 rng(42);

    constexpr size_t points = MAX_KB / STEP_KB;

    std::vector<double> sum_latency(MAX_KB / STEP_KB);

    for (int exp_n = 0; exp_n < RUNS; ++ exp_n) {

        for (size_t sz_kb = STEP_KB; sz_kb <= MAX_KB; sz_kb += STEP_KB) {
            size_t sz = sz_kb << 10;
            size_t n = sz / sizeof(Node);
            if (n < 2) n = 2;

            Node *buff = reinterpret_cast<Node *>(ALLOC(n * sizeof(Node)));
            if (!buff) {
                fprintf(stderr, "Allocation failed for size %zu\n", sz);
                return;
            }

            std::vector<size_t> idx(n);
            for (size_t i = 0; i < n; ++i) {
                idx[i] = i;
            }
            std::shuffle(idx.begin(), idx.end(), rng);
            for (size_t i = 0; i < n; ++i) {
                buff[idx[i]].next = &buff[idx[(i + 1) % n]];
            }

            // flushing cache lines
            for (size_t i = 0; i < n; ++i) {
                _mm_clflush(&buff[i]);
            }

            Node *p = &buff[idx[0]];
            uint64_t t0 = rdtscp();
            for (uint64_t it = 0; it < ITER; ++it) {
                p = p->next;
            }

            uint64_t t1 = rdtscp();
            FILE* null_file = fopen("/dev/null", "w");
            if (null_file != NULL) {
                fprintf(null_file, "%p", (void*)p);
                fclose(null_file);
            }
            double cyc = double(t1 - t0) / double(ITER);
            sum_latency[sz_kb / STEP_KB - 1] += cyc;

            FREE(buff);
        }
    }


   // printf("%s\n", "Size (KB),Avg cycles/access");
    double prev_avg = 0.0;
    double max_ratio = 0.0;
    size_t detected_kb = 0;

    //std::cout << "Size(KB), Avg cycles/access\n";

    for (size_t i = 0; i < points; ++i) {
        size_t sz_kb = (i + 1) * STEP_KB;
        double avg = sum_latency[i] / RUNS;

        //std::cout << sz_kb << "," << avg << "\n";

        if (i > 0 && prev_avg > 0) {
            double ratio = avg / prev_avg;

            if (ratio > max_ratio && ratio > 1.3) {
                max_ratio = ratio;
                detected_kb = (i) * STEP_KB;
            }
        }
        prev_avg = avg;
    }

    if (detected_kb > 0) {
        std::cout << "Cache size = " << detected_kb << " KB" << std::endl;
        cache_kb = detected_kb;
    } else {
        std::cout << "Cache size not detected reliably.\n";
    }
}



using namespace std::chrono;

constexpr int iterations = 50'000'000;

void stress(std::atomic<int>* a) {
    for (int i = 0; i < iterations; ++i)
        a->fetch_add(1, std::memory_order_relaxed);
}

size_t cache_line_size() {
    const size_t max_test = 1024;
    const size_t alignment = 1024;

    void* ptr = nullptr;
    if (posix_memalign(&ptr, alignment, max_test + sizeof(std::atomic<int>) * 2) != 0) {
        perror("posix_memalign failed");
        return 0;
    }

    char* buffer = static_cast<char*>(ptr);

    size_t best_line = 0;
    double prev_time = 0;
    bool found = false;

    for (size_t offset = sizeof(std::atomic<int>); offset < max_test; offset *= 2) {
        std::atomic<int>* a1 = reinterpret_cast<std::atomic<int>*>(buffer);
        std::atomic<int>* a2 = reinterpret_cast<std::atomic<int>*>(buffer + offset);

        a1->store(0, std::memory_order_relaxed);
        a2->store(0, std::memory_order_relaxed);

        auto t0 = high_resolution_clock::now();
        std::thread t1(stress, a1);
        std::thread t2(stress, a2);
        t1.join();
        t2.join();
        auto t1_end = high_resolution_clock::now();

        double time_ms = duration<double, std::milli>(t1_end - t0).count();

        if (prev_time > 0 && time_ms < prev_time * 0.7 && !found) {
            best_line = offset;
            found = true;
        }

        prev_time = time_ms;
      //  std::cout << "offset = " << offset << " bytes, time = " << time_ms << " ms\n";
    }

    free(ptr);
    return found ? best_line : 64;
}

int main() {
    cache_size();
    cache_assoc();
    size_t line = cache_line_size();
    printf(" cache line size = %zu bytes\n", line);
    return 0;
}


