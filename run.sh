#!/bin/sh
cmake CMakeList.txt
make cache_line_test
./cache_line_test