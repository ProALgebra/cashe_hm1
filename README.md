# cashe_hm1

First run 
//  cmake CMakeList.txt
then

make L1D_cache_size

./L1D_cache_size

then

make cache_line_size

./cache_line_size

then in casheutil.h in define write 

#define CACHE_LINE_SIZE \<your cache_line_size>


make L1D_assoc

./L1D_assoc \<cashe size> 30
