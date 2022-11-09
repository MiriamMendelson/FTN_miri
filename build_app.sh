#!/bin/bash
set -e

gcc -static -Wall -Wextra -Werror -o ./out_bin/app ./*.c -L./out_bin -lFTN