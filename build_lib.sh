#!/bin/bash
set -e

gcc -fsanitize=address -fsanitize=undefined -Wall -Wextra -Werror -c -o ./out_bin/FTN.o ./FTN/FTN_lib.c -g
ar rcs ./out_bin/libFTN.a ./out_bin/FTN.o
