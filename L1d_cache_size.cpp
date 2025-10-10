#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>
#include <random>
#include <malloc.h>
#include "cacheutil.h"
#include <iostream>

const size_t   STEP_KB = 2;             // 16 KB step
const size_t   MAX_KB  = 256;            // we assume that our L1D cache is no larger than 256 KB
const uint64_t ITER    = 10'000'000;
const size_t   RUNS    = 10;

int main() {

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
                return 1;
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


    printf("%s\n", "Size (KB),Avg cycles/access");
    for (size_t i = 0; i < points; ++i) {
        size_t sz_kb = (i + 1) * STEP_KB;
        double avg = sum_latency[i] / RUNS;
        printf("%zu,%.2f\n", sz_kb, avg);
        if( i > 1 && sum_latency[i] / sum_latency[i-1] > 1.5){
        size_t sz_kb = (i) * STEP_KB;
            std::cout << "cashe size = " << sz_kb << "kb" << std::endl;
            break;
        }
    }

    return 0;
}
