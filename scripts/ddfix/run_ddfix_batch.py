import os
import logging
import argparse
import subprocess


def parse_args():
    # Example usage: python3 run_ddfix_batch.py --program-name date-8.21
    parser = argparse.ArgumentParser(description="A python script to batch run DDFix (delta-debugging-fixer) on crash inputs.")

    parser.add_argument("--program-name", type=str, help="Name of the program to be repaired (e.g. date-8.21)", required=True, metavar="NAME")

    parser.add_argument("--only-test-reproduce", type=str, help="Only count how many crashes under given path can be reproduced (without running DDFix)", metavar="DEBLOATING_TOOL_NAME")
    parser.add_argument("--record-reproduction-output", action="store_true", help="Record the output of reproduction script for analysis or comparison (only work with --only-test-reproduce)")
    parser.add_argument("--program-to-test", type=str, help="Specify the path of the program (only work with --only-test-reproduce)", metavar="PATH")
    parser.add_argument("--test-argv", action="store_true", help="True for testing argv-fuzz, False for testing input-fuzz")
    parser.add_argument("--dry-run", action="store_true", help="Perform a dry run with no changes made")
    parser.add_argument("--no-log", action="store_true", help="Do not log to file")

    args = parser.parse_args()

    # hardcoded path (read from env.default.sh)
    script_dir = os.path.dirname(os.path.realpath(__file__))
    env = subprocess.check_output(f"bash -c '. {script_dir}/../../env.default.sh; env'", shell=True).decode("utf-8")
    env = dict(line.split("=", 1) for line in env.splitlines())
    args.crash_repair_path = env["CRASH_REPAIR_DIR"]  # for validation scripts
    args.deb_vul_repair_path = env["DEB_VUL_REPAIR_DIR"]  # for crash inputs
    args.analysis_tools_path = env["ANALYSIS_TOOLS_DIR"]

    return args


if __name__ == '__main__':
    args = parse_args()
    logger = logging.getLogger(__name__)
    if args.no_log:
        logger.disabled = True
    else:
        logging.basicConfig(filename=f"ddfix.{args.program_name}.log", level=logging.INFO, format="%(message)s")

    for debloating_tool_folder in os.listdir(args.deb_vul_repair_path):
        # if not debloating_tool_folder in ("chisel"):
        if not debloating_tool_folder in ("chisel", "blade", "cov", "cova", "covf"):
            continue
        if args.only_test_reproduce and not debloating_tool_folder == args.only_test_reproduce:
            continue
        debloating_tool_path = os.path.join(args.deb_vul_repair_path, debloating_tool_folder)

        for program_folder in os.listdir(debloating_tool_path):
            if not program_folder.startswith(args.program_name):
                continue
            # if not "argv" in program_folder:
            #     continue
            if args.only_test_reproduce:
                if args.test_argv and not "argv" in program_folder:
                    continue
                if not args.test_argv and "argv" in program_folder:
                    continue
            base_path = os.path.join(debloating_tool_path, program_folder)
            reproduce_count = 0
            crash_inputs_path = os.path.join(base_path, "crashed_inputs")
            logger.info(f'Start fixing "{os.path.join(base_path, args.program_name + ".c.reduced.c")}"')

            # list folders under that starts with "crash_"
            for crash_input_dir in os.listdir(crash_inputs_path):
                if not crash_input_dir.startswith("crash_"):
                    continue
                crash_input_path = os.path.join(crash_inputs_path, crash_input_dir)

                # select the first crash input
                for crash_input in os.listdir(crash_input_path):
                    if crash_input.startswith("id") or crash_input.startswith("input"):
                        break
                fuzzer_type = "afl" if crash_input.startswith("id") else "radamsa"
                with open(os.path.join(crash_input_path, "sanitizer_info"), "r") as f:
                    sanitizer_info = f.read().splitlines()
                    sanitizer_type = "nosan" if "nosan" in sanitizer_info else sanitizer_info[0]

                # prepare reproduce.sh
                reproduce_sh_path = os.path.join(crash_input_path, "reproduce.sh")
                if not os.path.exists(reproduce_sh_path):
                    with open(reproduce_sh_path, "w") as f:
                        f.write("#!/bin/bash\n")
                        f.write("BIN_FILE=$(realpath $1)\n")
                        f.write('SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )\n')
                        # f.write(f"exit 0\n")
                        f.write(f"timeout -k 0.5 0.5 $BIN_FILE < $SCRIPT_DIR/{crash_input}\n")

                # choose compile.sh
                # if fuzzer_type == "afl":
                #     compile_sh = "compile_afl.sh"
                compile_sh = "compile.sh" if sanitizer_type == "nosan" else f"compile_{sanitizer_type}.sh"

                if args.dry_run:
                    print(log_msg)
                    logger.info(f'  Fix crash input "{os.path.join(crash_input_dir, crash_input)}"')
                elif not args.only_test_reproduce:
                    # run DDFix
                    logger.info(f'  Fix crash input "{os.path.join(crash_input_dir, crash_input)}"')
                    # escape the path, or use subprocess (https://stackoverflow.com/questions/21822054)
                    program_name_no_version = args.program_name.split("-")[0]
                    os.system('bash -c ". {0}; bash {1} {2} {3} {4} {5} {6} {7} {8} {9}"'.format(
                        os.path.join(args.crash_repair_path, "setenv"),
                        os.path.join(args.analysis_tools_path, "scripts", "ddfix", "run_ddfix.sh"),
                        os.path.join(base_path, args.program_name + ".c.origin.c"),
                        os.path.join(base_path, args.program_name + ".c.reduced.c"),
                        f'\\"{os.path.join(crash_input_path, "fix_delta")}\\"',
                        f'\\"{reproduce_sh_path}\\"',
                        os.path.join(args.crash_repair_path, f"benchmark/{program_name_no_version}-argv-fuzz/test/runDebInputs.sh"),
                        os.path.join(args.analysis_tools_path, "scripts", "compile", compile_sh),
                        # fuzzer_type,
                        "radamsa",
                        sanitizer_type))
                else:
                    # only test reproduce
                    logger.info(f'  Test crash reproduction of input "{os.path.join(crash_input_dir, crash_input)}"')
                    program_to_test = args.program_to_test or os.path.join(base_path, args.program_name + ".c.reduced.c")
                    exit_code = os.system('bash -c "bash {0} {1} {2} {3} {4} {5} {6}"'.format(
                        os.path.join(args.analysis_tools_path, "scripts", "ddfix", "test_reproduce.sh"),
                        program_to_test,
                        f'\\"{reproduce_sh_path}\\"',
                        os.path.join(args.analysis_tools_path, "scripts", "compile", compile_sh),
                        # fuzzer_type,
                        "radamsa",
                        sanitizer_type,
                        "--output-to-file" if args.record_reproduction_output else ""))
                    if exit_code == 0:
                        logger.info(f"    Reproduced {sanitizer_type} {crash_input_path}")
                        reproduce_count += 1

            if not args.dry_run and args.only_test_reproduce:
                print(reproduce_count)
            logger.info(f'Finished fixing "{os.path.join(base_path, args.program_name + ".c.reduced.c")}"\n\n')
