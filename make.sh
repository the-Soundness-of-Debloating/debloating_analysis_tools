#!/bin/bash

PARALLEL_JOBS=$1
if [[ ! -z $PARALLEL_JOBS ]]; then PARALLEL_JOBS="-j $1"; else PARALLEL_JOBS="-j 4"; fi

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
cd $SCRIPT_DIR

mkdir -p build && cd build

CC=clang-15 CXX=clang++-15 cmake -DCUSTOM_LINK_OPTIONS="-fuse-ld=lld-15" -G Ninja ..
ninja $PARALLEL_JOBS
