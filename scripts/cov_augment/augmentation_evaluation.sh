#!/bin/bash

# Usage: bash augmentation_evaluation_full.sh <program_name>

program_name=$1  # e.g.: date-8.21

# TODO options
should_run_cov=1
should_check_related_input=1
should_evaluate_generality=1
should_evaluate_crash=1


SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
source $SCRIPT_DIR/../../env.default.sh

src1=$(realpath $program_name.c.reduced.c)  # before augmentation
src2=$(realpath $program_name.c.reduced.augmented.c)  # after augmentation
src_original=$(realpath $program_name.c.origin.c)

program_name_without_version=$(echo $program_name | cut -d- -f1)
base_dir=$(realpath "${program_name_without_version}_evaluation")
rm -rf $base_dir; mkdir -p $base_dir

result_file="$base_dir/result.txt"

cov_augment="$ANALYSIS_TOOLS_DIR/build/bin/cov_augment"
get_debloated_lines="bash $ANALYSIS_TOOLS_DIR/scripts/utils/get_debloated_lines.sh"
run_ddfix_batch="python3 $ANALYSIS_TOOLS_DIR/scripts/ddfix/run_ddfix_batch.py"
compiler="bash $ANALYSIS_TOOLS_DIR/scripts/compile/compile.sh"


evaluate_generality() {
    $compiler $src1 $base_dir/bin1.out
    $compiler $src2 $base_dir/bin2.out
    $compiler $src_original $base_dir/bin_original.out

    # Use qixin5/debloating_study repo for generality experiment. Print the differences between two programs.
    mkdir -p $base_dir/generality
    pushd $base_dir/generality > /dev/null
    mkdir -p $base_dir/dir1 $base_dir/dir2 $base_dir/dir_original
    for script in $DEBLOATING_STUDY_DIR/expt/debaug/benchmark/$program_name/testscript/I*/*; do
        # outdir
        testid=$(basename $(dirname $script))
        mkdir -p dir1/output/$testid dir2/output/$testid dir_original/output/$testid
        mkdir -p dir1/output_tmp/$testid dir2/output_tmp/$testid dir_original/output_tmp/$testid
        # indir
        input_origin=$DEBLOATING_STUDY_DIR/expt/debaug/benchmark/$program_name/input.origin

        # check if this input is related to the debloating specification
        if [ -d $input_origin/$testid ]; then
            if [ $program_name == "make-3.79" ]; then
                cp -r -p $input_origin/$testid input
            else
                cp -r $input_origin/$testid input
            fi
        fi
        tmp_out_dir=$(realpath dir1/output_tmp/$testid)
        tmp_dir=workdir
        rm -rf $tmp_dir; mkdir -p $tmp_dir/input
        pushd $tmp_dir > /dev/null
        create_wrapper_to_check_related eval_bin.py $base_dir/debloating_specification_inputs.txt is_related
        bash $script ./eval_bin.py $tmp_out_dir 1 $input_origin/$testid &>/dev/null
        popd > /dev/null
        if [[ ! -f $tmp_dir/is_related ]]; then
            rm -rf $tmp_dir
            if [[ $should_check_related_input == "1" ]]; then
                continue
            fi
        fi

        # compile (testing rm may get the binary removed; same filename for same "usage" output)
        # run script with args: "binary outdir timeout indir"
        for bin in 1 2 _original; do
            out_dir=$(realpath dir${bin}/output/$testid)
            tmp_out_dir=$(realpath dir${bin}/output_tmp/$testid)

            tmp_dir=workdir
            rm -rf $tmp_dir; mkdir -p $tmp_dir/input
            pushd $tmp_dir > /dev/null

            if [ -d $input_origin/$testid ]; then
                if [ $program_name == "make-3.79" ]; then
                    cp -r -p $input_origin/$testid input
                else
                    cp -r $input_origin/$testid input
                fi
            fi

            # echo "Running $program_name bin${bin}.out on $testid ($(date +%H:%M:%S))"

            cp $base_dir/bin${bin}.out eval_bin
            bash $script ./eval_bin $out_dir 1 input/$testid &>/dev/null

            popd > /dev/null
            rm -rf $tmp_dir
        done
        # copy outputs to base_dir
        cp -r dir1/output $base_dir/dir1
        cp -r dir2/output $base_dir/dir2
        cp -r dir_original/output $base_dir/dir_original
    done
    popd > /dev/null

    # count files that do not contain "INPUT_NOT_IN_SPEC"
    total_count=$(find $base_dir/dir_original/output -type f | xargs grep -L "INPUT_NOT_IN_SPEC" | wc -l)
    differ_count_1=$(diff -qr $base_dir/dir1/output $base_dir/dir_original/output | grep -c "^Files.*differ$")
    differ_count_2=$(diff -qr $base_dir/dir2/output $base_dir/dir_original/output | grep -c "^Files.*differ$")
    echo | tee -a $result_file
    echo "Generality problems count (before augmentation): $differ_count_1/$total_count" | tee -a $result_file
    echo "Generality problems count (after augmentation): $differ_count_2/$total_count" | tee -a $result_file
}

# TODO Not finished
evaluate_generality_domgad() {
    $compiler $src1 $base_dir/bin1.out
    $compiler $src2 $base_dir/bin2.out
    $compiler $src_original $base_dir/bin_original.out

    mkdir -p $base_dir/generality_domgad
    pushd $base_dir/generality_domgad > /dev/null
    mkdir -p $base_dir/dir1 $base_dir/dir2 $base_dir/dir_original
    for argfile in /workspace/workspace-username/Domgad/workdir/uniq_samples/arg/*; do
        # outdir
        testid=$(basename $argfile)
        mkdir -p dir1/output dir2/output dir_original/output

        echo "Test id: $testid"

        # compile (testing rm may get the binary removed; same filename for same "usage" output)
        for bin in 1 2 _original; do
            out_path=$(realpath dir${bin}/output/$testid)

            tmp_dir=workdir
            rm -rf $tmp_dir; mkdir -p $tmp_dir/input
            pushd $tmp_dir > /dev/null

            # echo "Running $program_name bin${bin}.out on $testid ($(date +%H:%M:%S))"

            # check if this input is related to the debloating specification
            tmp_dir=workdir
            rm -rf $tmp_dir; mkdir -p $tmp_dir/input
            pushd $tmp_dir > /dev/null
            create_wrapper_to_check_related eval_bin.py $base_dir/debloating_specification_inputs.txt is_related
            argstr=`head -n 1 ${argfile}`
            ./eval_bin.py $argstr &>/dev/null
            popd > /dev/null
            if [[ ! -f $tmp_dir/is_related ]]; then
                rm -rf $tmp_dir
                if [[ $should_check_related_input == "1" ]]; then
                    continue
                fi
            fi

            cp $base_dir/bin${bin}.out eval_bin

            argstr=`head -n 1 ${argfile}`

            if [ -f /workspace/workspace-username/Domgad/workdir/uniq_samples/file/runarg${testid}.txt ]; then
                argf=`head -n 1 /workspace/workspace-username/Domgad/workdir/uniq_samples/file/runarg${testid}.txt`
                # echo "Command: ./eval_bin ${argstr} /workspace/workspace-username/Domgad/workdir/uniq_samples/file/${argf} &> $out_path"
                echo "timeout -k 1 1 ./eval_bin ${argstr} /workspace/workspace-username/Domgad/workdir/uniq_samples/file/${argf} &> $out_path" >run.sh
                chmod 700 run.sh && ./run.sh
            else
                # echo "Command: ./eval_bin ${argstr} &> $out_path"
                echo "timeout -k 1 1 ./eval_bin ${argstr} &> $out_path" >run.sh
                chmod 700 run.sh && ./run.sh
            fi

            popd > /dev/null
            rm -rf $tmp_dir
        done
        # copy outputs to base_dir
        cp -r dir1/output $base_dir/dir1
        cp -r dir2/output $base_dir/dir2
        cp -r dir_original/output $base_dir/dir_original
    done
    popd > /dev/null

    # count files that do not contain "INPUT_NOT_IN_SPEC"
    total_count=$(find $base_dir/dir_original/output -type f | xargs grep -L "INPUT_NOT_IN_SPEC" | wc -l)
    differ_count_1=$(diff -qr $base_dir/dir1/output $base_dir/dir_original/output | grep -c "^Files.*differ$")
    differ_count_2=$(diff -qr $base_dir/dir2/output $base_dir/dir_original/output | grep -c "^Files.*differ$")
    echo | tee -a $result_file
    echo "Generality problems count (before augmentation): $differ_count_1/$total_count" | tee -a $result_file
    echo "Generality problems count (after augmentation): $differ_count_2/$total_count" | tee -a $result_file
}

evaluate_size() {
    lines1=$(cat $src1 | grep -v "^\s*$" | wc -l)
    lines2=$(cat $src2 | grep -v "^\s*$" | wc -l)
    lines_original=$(cat $src_original | grep -v "^\s*$" | wc -l)
    echo | tee -a $result_file
    echo -n "Line count increment: " | tee -a $result_file
    if [[ $lines1 -gt $lines2 ]]; then echo -n $((lines1 - lines2)); else echo -n $((lines2 - lines1)) | tee -a $result_file; fi
    echo -n "/" | tee -a $result_file
    if [[ $lines1 -gt $lines_original ]]; then echo -n $((lines1 - lines_original)); else echo -n $((lines_original - lines1)) | tee -a $result_file; fi
    echo " (original: $lines_original, before: $lines1, after: $lines2)" | tee -a $result_file
}

evaluate_crash() {
    pushd $base_dir > /dev/null

    # Vulnerability comparison
    # find main function in the source code (may not be "int main" but "IntNative main")
    main1=$(grep -n "main.*(.*{" $src1 | head -n 1 | cut -d: -f1)
    main2=$(grep -n "main.*(.*{" $src2 | head -n 1 | cut -d: -f1)
    main_original=$(grep -n "main.*(.*{" $src_original | head -n 1 | cut -d: -f1)
    if [[ -z $main1 || -z $main2 || -z $main_original ]]; then
        echo "ERROR: Main function not found."
        exit 1
    fi
    # insert macro call after
    src1_argv=$(realpath src1.argv.tmp.c)
    src2_argv=$(realpath src2.argv.tmp.c)
    src_original_argv=$(realpath src_original.argv.tmp.c)
    sed "${main1}a \ \ AFL_INIT_SET0(\"$program_name_without_version\");" $src1 > $src1_argv
    sed "${main2}a \ \ AFL_INIT_SET0(\"$program_name_without_version\");" $src2 > $src2_argv
    sed "${main_original}a AFL_INIT_SET0(\"$program_name_without_version\");" $src_original > $src_original_argv
    # insert include argv-fuzz-inl.h before
    sed -i "${main1}i #include \"argv-fuzz-inl.h\"" $src1_argv
    sed -i "${main2}i #include \"argv-fuzz-inl.h\"" $src2_argv
    sed -i "${main_original}i #include \"argv-fuzz-inl.h\"" $src_original_argv
    # test crash reproduction
    mkdir -p dir1/output_crash dir2/output_crash dir_original/output_crash
    pushd dir1/output_crash > /dev/null
    crash1=$($run_ddfix_batch --only-test-reproduce cov --record-reproduction-output --program-to-test $src1_argv --test-argv --program-name $program_name | tail -n 1)
    popd > /dev/null
    pushd dir2/output_crash > /dev/null
    crash2=$($run_ddfix_batch --only-test-reproduce cov --record-reproduction-output --program-to-test $src2_argv --test-argv --program-name $program_name | tail -n 1)
    popd > /dev/null
    pushd dir_original/output_crash > /dev/null
    crash_original=$($run_ddfix_batch --only-test-reproduce cov --record-reproduction-output --program-to-test $src_original_argv --test-argv --program-name $program_name | tail -n 1)
    popd > /dev/null
    echo | tee -a $result_file
    echo "Crash count (before augmentation): $crash1" | tee -a $result_file
    echo "Crash count (after augmentation): $crash2" | tee -a $result_file
    echo "Crash count (original): $crash_original" | tee -a $result_file

    # # Error handling comparison (also uses crash inputs)
    # echo | tee -a $result_file
    # totalSimilarity=0
    # totalFiles=0
    # for f1 in dir1/output_crash/*; do
    #     fname=$(basename $f1)
    #     if [[ $fname != "crash_"* ]]; then continue; fi
    #     fo=dir_original/output_crash/$fname
    #     if [[ ! -f $fo ]]; then continue; fi
    #     identicalLines=$(diff --unchanged-line-format='%L' --old-line-format='' --new-line-format='' $f1 $fo | wc -l)
    #     totalLines=$(bc <<< "scale=4; $(wc -l < $f1) + $(wc -l < $fo)")
    #     if [[ $totalLines -eq 0 ]]; then continue; fi
    #     similarityPercentage=$(bc <<< "scale=4; 2 * $identicalLines / $totalLines")
    #     totalSimilarity=$(bc <<< "scale=4; $totalSimilarity + $similarityPercentage")
    #     ((totalFiles++))
    # done
    # echo "Average similarity (before augmentation): $(bc <<< "scale=2; $totalSimilarity / $totalFiles * 100")%" | tee -a $result_file
    # totalSimilarity=0
    # totalFiles=0
    # for f2 in dir2/output_crash/*; do
    #     fname=$(basename $f2)
    #     if [[ $fname != "crash_"* ]]; then continue; fi
    #     fo=dir_original/output_crash/$fname
    #     if [[ ! -f $fo ]]; then continue; fi
    #     identicalLines=$(diff --unchanged-line-format='%L' --old-line-format='' --new-line-format='' $f2 $fo | wc -l)
    #     totalLines=$(bc <<< "scale=4; $(wc -l < $f2) + $(wc -l < $fo)")
    #     if [[ $totalLines -eq 0 ]]; then continue; fi
    #     similarityPercentage=$(bc <<< "scale=4; 2 * $identicalLines / $totalLines")
    #     totalSimilarity=$(bc <<< "scale=4; $totalSimilarity + $similarityPercentage")
    #     ((totalFiles++))
    # done
    # echo "Average similarity (after augmentation): $(bc <<< "scale=2; $totalSimilarity / $totalFiles * 100")%" | tee -a $result_file

    popd > /dev/null
}

create_wrapper_to_get_args() {
    py_script_path=$1
    deb_spec_file_path=$2
    awk -v deb_spec_file_path=$deb_spec_file_path \
        '{ gsub("{{deb_spec_file_path}}", deb_spec_file_path); print }' \
        $SCRIPT_DIR/wrapper_template_get_args.py > $py_script_path
    chmod +x $py_script_path
}
create_wrapper_to_run_binary() {
    # Only run the binary when the input is related to the recorded arguments.
    py_script_path=$1
    deb_spec_file_path=$2
    bin_path=$3
    awk -v deb_spec_file_path=$deb_spec_file_path -v bin_path=$bin_path \
        '{ gsub("{{deb_spec_file_path}}", deb_spec_file_path); gsub("{{bin_path}}", bin_path); print }' \
        $SCRIPT_DIR/wrapper_template_run_binary.py > $py_script_path
    chmod +x $py_script_path
}
create_wrapper_to_check_related() {
    py_script_path=$1
    deb_spec_file_path=$2
    result_path=$3
    awk -v deb_spec_file_path=$deb_spec_file_path -v result_path=$result_path -v program_name=$program_name \
        '{ gsub("{{deb_spec_file_path}}", deb_spec_file_path); gsub("{{result_path}}", result_path); gsub("{{program_name}}", program_name); print }' \
        $SCRIPT_DIR/wrapper_template_check_related.py > $py_script_path
    chmod +x $py_script_path
}


echo
echo "---- Perform Cov debloating on $program_name ----"
pushd $DEBAUG_DIR/chiselbench/$program_name > /dev/null
if [[ $should_run_cov == "1" ]]; then
    $DEBAUG_DIR/bin/getprog/getprog_cov.sh $program_name testscript
fi
create_wrapper_to_get_args $base_dir/argument_getter.py $base_dir/debloating_specification_inputs.txt
for script in testscript/*; do
    $script $base_dir/argument_getter.py
done
popd > /dev/null

echo
echo "---- Perform augmentation on $program_name ----"
cp $DEBAUG_DIR/chiselbench/$program_name/src/origin/$program_name.c.origin.c $src_original
cp $DEBAUG_DIR/chiselbench/$program_name/src/reduced/testscript_cov/$program_name.nodce.c $src1
$get_debloated_lines $src1 $src_original $program_name.debloated_lines.txt
# --aug-strat=exit,sym_assign,keyword
$cov_augment --aug-strat=exit --debloated-src=$src1 --debloated-lines=$program_name.debloated_lines.txt $src_original --
rm $program_name.debloated_lines.txt

echo
echo "---- Perform evaluation ----"
evaluate_size
if [[ $should_evaluate_generality == "1" ]]; then
    evaluate_generality
    # evaluate_generality_domgad
fi
if [[ $should_evaluate_crash == "1" ]]; then
    evaluate_crash
fi

echo
echo "---- Perform DCE on $program_name (SKIPPED) ----"
