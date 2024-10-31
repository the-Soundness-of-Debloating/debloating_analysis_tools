#!/bin/bash

if [[ $# -lt 5 ]]; then echo "Missing arguments." && exit 1; fi

DEBLOATED_SRC=$(realpath $1)
RUN_SCRIPT=$(realpath $2)
COMPILE_SCRIPT=$(realpath $3)
FUZZER_TYPE=$4  # "afl", "radamsa", "other"
SANITIZER_TYPE=$5  # "nosan", "asan", "ubsan", "msan", "tsan", "lsan"
if [[ $# -eq 6 && $6 == "--output-to-file" ]]; then OUTPUT_TO_FILE=1; fi

if [[ $FUZZER_TYPE == "afl" ]]; then
    unset AFL_USE_ASAN
    unset AFL_USE_CFISAN
    unset AFL_USE_LSAN
    unset AFL_USE_MSAN
    unset AFL_USE_TSAN
    unset AFL_USE_UBSAN
    if [[ $SANITIZER_TYPE != "nosan" ]]; then
        export AFL_USE_$(echo "$SANITIZER_TYPE" | tr '[:lower:]' '[:upper:]')=1
    fi
else
    export ASAN_OPTIONS='abort_on_error=1'
    export UBSAN_OPTIONS='abort_on_error=1'
    export TSAN_OPTIONS='abort_on_error=1'
    export MSAN_OPTIONS='abort_on_error=1'
    export LSAN_OPTIONS='abort_on_error=1'
fi


# Try to reproduce the crash/hang in the debloated program
bash $COMPILE_SCRIPT $DEBLOATED_SRC $DEBLOATED_SRC.test.temp &> /dev/null
if [[ OUTPUT_TO_FILE -eq 1 ]]; then
    crash_id=$(echo $RUN_SCRIPT | cut -d: -f1 | rev | cut -d/ -f1 | rev)
    bash $RUN_SCRIPT $DEBLOATED_SRC.test.temp &> ${crash_id}_output
else
    bash $RUN_SCRIPT $DEBLOATED_SRC.test.temp &> /dev/null
fi
# crash = ((retcode >= 131 && retcode <= 136) || retcode == 139)
# hang = (retcode == 124 || retcode == 137)
if [[ $? -eq 124 || $? -eq 137 || ($? -ge 131 && $? -le 136) || $? -eq 139 ]]; then
    rm $DEBLOATED_SRC.test.temp
    exit 0
else
    rm $DEBLOATED_SRC.test.temp
    exit 1
fi
