#!/bin/bash
set -e

gcc -Wall -Wextra -Werror -c -o ./out_bin/FTN.o ./FTN/*.c
ar rcs ./out_bin/libFTN.a ./out_bin/FTN.o
