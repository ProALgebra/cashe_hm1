#!/bin/sh
cmake CMakeLists.txt
make cache_line_test
./cache_line_test