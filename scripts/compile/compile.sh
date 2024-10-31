#!/bin/bash

SRC=$1
BIN=$2
flags=$3

if [ -z $4 ]; then
    COMPILER=clang #Default
else
    COMPILER=$4
fi

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
SRC_DIR=$(dirname $SRC)

if [[ $SRC =~ "grep" ]]; then
    flags="-lpcre -D __msan_unpoison(s,z) $flags"
elif [[ $SRC =~ "sort" ]]; then
    flags="-lpthread $flags"
fi

$COMPILER -I$SRC_DIR -I$SCRIPT_DIR ${flags} -w -o $BIN $SRC
