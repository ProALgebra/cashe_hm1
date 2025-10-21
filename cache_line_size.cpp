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
#include <bits/stdc++.h>
#include <x86intrin.h>



size_t cache_kb = 32;
size_t max_assoc = 15;
const size_t   STEP_KB = 2;
const size_t   MAX_KB  = 256;
const uint64_t ITER    = 10'000'000;
const int RUNS = 10;


void cache_assoc(){
    size_t cache_bytes = cache_kb << 10;
    constexpr uint64_t ITER = 10'000'000;
    double last =0;
    //printf("Assoc,Cycles/(access),Stride\n");
    int maxi = 0;
    last = 0;
    int index = 0;
    for (size_t assoc_guess = 1; assoc_guess <= max_assoc; ++assoc_guess) {
        size_t stride = cache_bytes * assoc_guess;

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
        std::cout << "Estimated cache size = " << detected_kb << " KB" << std::endl;
        cache_kb = detected_kb;
    } else {
        std::cout << "Cache size not detected reliably.\n";
    }
}

static inline uint64_t rdtsc_serialized() {
    unsigned aux;
    _mm_lfence();
    uint64_t t = __rdtscp(&aux);
    _mm_lfence();
    return t;
}

void build_cycle(void **buf, size_t buf_bytes, size_t stride) {
    size_t nodes = buf_bytes / stride;
    char *base = (char*)buf;

    const size_t MAGIC = 7;
    const size_t OFFSET = 3;

    for (size_t i = 0; i < nodes; ++i) {
        size_t next = (i * MAGIC + OFFSET) % nodes;
        void *node = base + i * stride;
        void *nextnode = base + next * stride;
        *(void**)node = nextnode;
    }
}

static inline const void* chase(const void *start, size_t COUNT) {
    const void *p = start;
    for (size_t i = 0; i < COUNT; ++i)
        p = *(const void**)p;
    return p;
}

size_t cache_line_size() {
    const size_t PAGE = 4096;
    const size_t BUF_BYTES = 8ull * 1024 * 1024; // 8 MiB
    void *raw = nullptr;
    if (posix_memalign(&raw, PAGE, BUF_BYTES) != 0) {
        perror("posix_memalign");
        return 0;
    }
    void **buf = (void**)raw;

    const unsigned RUNS = 20;
    const size_t COUNT = 2'000'000;

    std::vector<size_t> strides;
    for (size_t s = sizeof(void*); s <= 1024; s <<= 1) strides.push_back(s);

    std::unordered_map<size_t, double> avg_cycles;
    for (size_t s : strides) avg_cycles[s] = 0.0;

    for (unsigned run = 0; run < RUNS; ++run) {
        for (size_t stride : strides) {
            if (stride < sizeof(void*)) continue;
            if (BUF_BYTES % stride != 0) continue;

            build_cycle(buf, BUF_BYTES, stride);
            chase(buf, 10000); // warmup

            uint64_t t0 = rdtsc_serialized();
            const void* p = chase(buf, COUNT);
            uint64_t t1 = rdtsc_serialized();

            volatile const void* sink = p;
            (void)sink;

            double cycles = double(t1 - t0) / double(COUNT);
            avg_cycles[stride] += cycles;
        }
    }

    for (size_t s : strides) avg_cycles[s] /= RUNS;

   // printf("Stride(bytes), AvgCyclesPerHop\n");
   // for (size_t s : strides)
 //       printf("%zu, %.3f\n", s, avg_cycles[s]);

    size_t detected_stride = 0;
    double max_ratio = 0.0;
    double prev = avg_cycles[strides.front()];
    for (size_t i = 1; i < strides.size(); ++i) {
        double cur = avg_cycles[strides[i]];
        double ratio = cur / prev;
        if (ratio > max_ratio) {
            max_ratio = ratio;
            detected_stride = strides[i];
        }
        prev = cur;
    }

   // if (detected_stride)
     //   printf("Estimated cache line size = %zu bytes (ratio %.2f)\n",
     //          detected_stride, max_ratio);
 //   else
       // printf("No clear cacheline step detected.\n");

    free(raw);
    return detected_stride;
}

int main() {
    size_t line = cache_line_size();
    printf("Detected cache line size = %zu bytes\n", line*2);
    cache_size();
    cache_assoc();
    return 0;
}



