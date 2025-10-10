#include <cstdio>
#include <cstdint>
#include <x86intrin.h>
#include <vector>
#include <iostream>

const int RUNS = 10000;
#include <vector>
#include <cmath>
#include <cstdio>



int main() {
    const size_t N = 1 << 20;
    alignas(64) int a[N];
    std::vector<double> avg_cycles(300);
    size_t idx;

    for (size_t i = 0; i < RUNS; ++i) {
        idx = 0;
        for (size_t stride = 1; stride <= 256; stride <<= 1) {
            uint64_t t0 = __rdtsc();
            for (size_t i = 0; i < N; i += stride) {
                a[i] += 1;
            }
            uint64_t t1 = __rdtsc();
            double cycles = double(t1 - t0) / (N / stride);
            avg_cycles[idx] += cycles;
            ++idx;
        }
    }

    idx = 0;
    int maxi = 2;
    int maxist = 0;
//    printf("stride,Avg cycles/access\n");
    for (size_t stride = 1; stride <= 256; stride <<= 1, ++idx) {
       // printf("%d ", stride);
      // printf("%4zu,%.2f\n", stride * sizeof(int), avg_cycles[idx] / RUNS);
        if(idx < 3){
            continue;
        }
      //  std::cout <<  avg_cycles[idx+1]   / RUNS << ' ';
      //  std::cout <<   avg_cycles[idx] / RUNS << ' ';
       // std::cout <<   (avg_cycles[idx+1]   / RUNS) / (avg_cycles[idx] / RUNS) << ' ';
       // std::cout <<   (avg_cycles[idx]   / RUNS) / (avg_cycles[idx - 1] / RUNS) ;
        if(avg_cycles[idx] / avg_cycles[idx-1] > avg_cycles[maxi] / avg_cycles[maxi-1]){

           // printf("%4zu,%.2f\n", stride * sizeof(int), avg_cycles[idx] / RUNS);
            maxi = idx;
    //std::cout << "cash line sizesss:" << maxi << std::endl;

            maxist = stride;
           // break;
        }

    }
    std::cout << "cash line size:" << maxist * sizeof(int) << std::endl;
    return 0;
}
