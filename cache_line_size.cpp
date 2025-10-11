#include <cstdio>
#include <cstdint>
#include <x86intrin.h>
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

size_t cache_kb = 48;
size_t cache_bytes = cache_kb << 10;
size_t max_assoc = 15;
const size_t   STEP_KB = 2;
const size_t   MAX_KB  = 256;
const uint64_t ITER    = 10'000'000;
const int RUNS = 10;


void cache_assoc(){
    constexpr uint64_t ITER = 10'000'000;
    double last =0;
    //printf("Assoc,Cycles/(access),Stride\n");
    int maxi = 0;
    last = 0;
    int index = 0;
    for (size_t assoc_guess = 1; assoc_guess <= max_assoc; ++assoc_guess) {
        size_t stride = cache_bytes / assoc_guess;

        size_t n = assoc_guess + 1;

        size_t alloc_bytes = stride * n + CACHE_LINE_SIZE;
        char* mem = (char*) ALLOC(alloc_bytes);
        if (!mem) {
            perror("malloc");
            return;
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
  //     printf("%4zu,%.2f,%6zu\n", assoc_guess, cyc, stride);
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

            // ищем самый сильный скачок в латентности
            if (ratio > max_ratio && ratio > 1.3) {
                max_ratio = ratio;
                detected_kb = (i) * STEP_KB;
            }
        }
        prev_avg = avg;
    }

    if (detected_kb > 0) {
        std::cout << "Estimated cache size ≈ " << detected_kb << " KB" << std::endl;
        cache_kb = detected_kb;
    } else {
        std::cout << "Cache size not detected reliably.\n";
    }
}

void cache_line_size(){
    const size_t N = 1 << 30; // 8M ints (~32MB)
    std::vector<int> a(N, 0);
    const size_t RUNS = 20;

    std::vector<double> avg_cycles(256, 0);

    for (size_t run = 0; run < RUNS; ++run) {
        for (size_t stride = 1; stride <= 256; stride <<= 1) {
            unsigned aux;
            _mm_lfence();
            uint64_t t0 = __rdtscp(&aux);
            _mm_lfence();

            volatile int *pa = a.data();
            for (size_t i = 0; i < N; i += stride)
                pa[i]++;

            _mm_lfence();
            uint64_t t1 = __rdtscp(&aux);
            _mm_lfence();

            double cycles = double(t1 - t0) / (N / stride);
            avg_cycles[stride] += cycles;
        }
    }

    size_t detected = 0;
    double prev = 0;
    double max_ratio = 0;
  //  std::cout << "Stride(bytes), AvgCycles\n";
    for (size_t stride = 1; stride <= 256; stride <<= 1) {
        double avg = avg_cycles[stride] / RUNS;
     //   std::cout << stride * sizeof(int) << "," << avg << "\n";
        double ratio = avg / prev;
    //    std::cout << ratio << ' ';
        if ( stride > 1 && ratio > max_ratio) {
            max_ratio = ratio;
            detected = stride;
        }
        prev = avg;
    }

    std::cout << "Estimated cache line size ≈ "
          << detected * sizeof(int) << " bytes\n";
}


int main() {
    cache_line_size();
    cache_size();
    cache_assoc();
    return 0;
}
