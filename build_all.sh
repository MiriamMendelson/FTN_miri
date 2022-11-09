#!/bin/bash
set -e

mkdir -p out_bin
rm -rf out_bin/*


./build_lib.sh
./build_app.sh
echo "build done!"