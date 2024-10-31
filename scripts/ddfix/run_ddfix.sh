#!/bin/bash

if [[ $# -lt 8 ]]; then echo "Missing arguments." && exit 1; fi

ORIGINAL_SRC=$(realpath $1)
DEBLOATED_SRC=$(realpath $2)
OUTPUT_DIR=$(realpath $3)
RUN_SCRIPT=$(realpath $4)
TEST_SCRIPT=$(realpath $5)
COMPILE_SCRIPT=$(realpath $6)
FUZZER_TYPE=$7  # "afl", "radamsa", "other"
SANITIZER_TYPE=$8  # "nosan", "asan", "ubsan", "msan", "tsan", "lsan"
EXTRA_ARGS=$9  # "--skip-reduction", "--no-redir" (remember to also change crash reproduction in this script)

# TIMEOUT=7200

PROG_NAME=$(basename $DEBLOATED_SRC .reduced.c)
SRC_DIR=$(dirname $ORIGINAL_SRC)

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
source $SCRIPT_DIR/../../env.default.sh

align_source_codes="python3 $ANALYSIS_TOOLS_DIR/scripts/utils/align_source_codes.py"
get_debloated_lines="bash $ANALYSIS_TOOLS_DIR/scripts/utils/get_debloated_lines.sh"
ddfix="$ANALYSIS_TOOLS_DIR/build/bin/fixer"


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


start_time=$(date +%s)


original_aligned_src=$ORIGINAL_SRC
debloated_aligned_src=$DEBLOATED_SRC


echo
echo
echo ---- Fixing $debloated_aligned_src ----


fix_dir=$OUTPUT_DIR
mkdir -p $fix_dir


# Try to reproduce the crash/hang in the original program
bash $COMPILE_SCRIPT $original_aligned_src $original_aligned_src.test.temp &> /dev/null
bash $RUN_SCRIPT $original_aligned_src.test.temp &> /dev/null
# crash = ((retcode >= 131 && retcode <= 136) || retcode == 139)
# hang = (retcode == 124 || retcode == 137)
if [[ $? -eq 124 || $? -eq 137 || ($? -ge 131 && $? -le 136) || $? -eq 139 ]]; then
    echo "The original program also crashes/hangs. Skipped fixing."
    echo "The original program also crashes/hangs. Skipped fixing." > $fix_dir/failed_to_reproduce.txt
    rm $original_aligned_src.test.temp
    exit 0
else
    rm $original_aligned_src.test.temp
fi


# Try to reproduce the crash/hang in the debloated program
bash $COMPILE_SCRIPT $debloated_aligned_src $debloated_aligned_src.test.temp &> /dev/null
bash $RUN_SCRIPT $debloated_aligned_src.test.temp &> /dev/null
# crash = ((retcode >= 131 && retcode <= 136) || retcode == 139)
# hang = (retcode == 124 || retcode == 137)
if [[ $? -eq 124 || $? -eq 137 || ($? -ge 131 && $? -le 136) || $? -eq 139 ]]; then
    echo "Reproduced the crash/hang. Start fixing."
    rm $debloated_aligned_src.test.temp
else
    echo "Failed to reproduce the crash/hang in the debloated program. Skipped fixing."
    echo "Failed to reproduce the crash/hang in the debloated program. Skipped fixing." > $fix_dir/failed_to_reproduce.txt
    rm $debloated_aligned_src.test.temp
    exit 0
fi


# echo ---- Step 1: Align Source Codes ----
# if [[ ! -f $SRC_DIR/$PROG_NAME.debloated.aligned.c ]]; then
#     $align_source_codes $DEBLOATED_SRC $ORIGINAL_SRC
#     mv $PROG_NAME.debloated.aligned.c $SRC_DIR
#     mv $PROG_NAME.original.aligned.c $SRC_DIR
# fi
# original_aligned_src=$SRC_DIR/$PROG_NAME.original.aligned.c
# debloated_aligned_src=$SRC_DIR/$PROG_NAME.debloated.aligned.c


echo
fixed_src=$(basename $debloated_aligned_src .c).fixed.c
$get_debloated_lines $debloated_aligned_src $original_aligned_src $PROG_NAME.debloated-lines
# timeout $TIMEOUT $ddfix $EXTRA_ARGS --original-src=$original_aligned_src --compile-script=$COMPILE_SCRIPT --reproduce-script=$RUN_SCRIPT --other-test-script=$TEST_SCRIPT --debloated-lines=$PROG_NAME.debloated-lines $debloated_aligned_src -- -Wall
$ddfix $EXTRA_ARGS --original-src=$original_aligned_src --compile-script=$COMPILE_SCRIPT --reproduce-script=$RUN_SCRIPT --other-test-script=$TEST_SCRIPT --debloated-lines=$PROG_NAME.debloated-lines $debloated_aligned_src -- -Wall
mv $fixed_src $fix_dir
rm $PROG_NAME.debloated-lines
fixed_src=$fix_dir/$fixed_src

end_time=$(date +%s)


# result_file=$fix_dir/result.txt
result_file=$fix_dir/result.txt
debloated_line_count=$(cat $debloated_aligned_src | sed '/^\s*$/d' | wc -l)
fixed_line_count=$(cat $fixed_src | sed '/^\s*$/d' | wc -l)
add_back_percentage=$(awk -v a="$fixed_line_count" -v b="$debloated_line_count" 'BEGIN{ print ((a - b) / b) }')
echo "Line count before fix: $debloated_line_count" > $result_file
echo "Line count after fix: $fixed_line_count" >> $result_file
echo "Add Back Lines Percentage: $add_back_percentage" >> $result_file
echo "Total Time: $(($end_time - $start_time))" >> $result_file
