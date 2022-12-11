#!/bin/bash
set -e

gcc -fsanitize=address -fsanitize=undefined -Wall -Wextra -Werror -o ./out_bin/app ./*.c -L./out_bin -lFTN -g -lrt