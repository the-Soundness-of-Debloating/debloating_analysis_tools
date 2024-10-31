#!/usr/bin/env python3


# This is a template script containing placeholders in the form of {{xxx}}. Replace them before use.
# The script is a wrapper for running a binary, but the binary will only be run if the arguments is related to the given specification.


import sys
import os
import json
import subprocess


# sys.exit(subprocess.run([r"{{bin_path}}"] + sys.argv[1:]).returncode)


def convert_args_to_options(args):
    options = set()
    for arg in args:
        if arg.startswith("--"):
            idx = arg.find("=")
            options.add(arg[:idx] if idx != -1 else arg)
        elif arg.startswith("-"):
            options.add(arg[:2])
    return options


options = convert_args_to_options(sys.argv[1:])

with open(r"{{deb_spec_file_path}}", "r") as f:
    lines = f.readlines()

for line in lines:
    args = json.loads(line)
    _options = convert_args_to_options(args)
    if options.issubset(_options):
        sys.exit(subprocess.run([r"{{bin_path}}"] + sys.argv[1:]).returncode)
print("INPUT_NOT_IN_SPEC")
sys.exit(1)
