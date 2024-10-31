#!/usr/bin/env python3


# This is a template script containing placeholders in the form of {{xxx}}. Replace them before use.
# The script is a wrapper for running a binary, but instead of running the binary, it checks if the arguments is related to the given specification.


import sys
import os
import json
import re

def convert_args_to_options(args):
    options = set()
    for arg in args:
        if arg.startswith("--"):
            idx = arg.find("=")
            options.add(arg[:idx] if idx != -1 else arg)
        elif arg.startswith("-"):
            options.add(arg[:2])

    # hardcoding for some tricky cases
    if r"{{program_name}}".startswith("date"):
        # "+%Y-%m-%d %H:%M:%S"
        if len(args) >= 1 and args[-1].startswith("+"):
            format_specifiers = re.findall(r"%[a-zA-Z]", args[-1])
            options = options.union(format_specifiers)
    elif r"{{program_name}}".startswith("chown"):
        # "user:group", "user", "user:", ":group"
        if not "--reference" in options and len(args) >= 2:
            user_group_arg = args[-2]
            if bool(':' in user_group_arg):
                user_arg = "user" if user_group_arg.split(':')[0] else ""
                group_arg = "group" if user_group_arg.split(':')[1] else ""
                options.add(f"{user_arg}:{group_arg}")
            else:
                options.add("user:")
    elif r"{{program_name}}".startswith("tar"):
        # can be "tar xvf" or "tar -xvf"
        if len(args) >= 1:
            option_arg = args[0]
            if option_arg.startswith("-"):
                options = options.union([f"-{opt}" for opt in option_arg[1:]])
            else:
                options = options.union([f"-{opt}" for opt in option_arg])
    return options


options = convert_args_to_options(sys.argv[1:])

with open(r"{{deb_spec_file_path}}", "r") as f:
    lines = f.readlines()

# with open(r"/workspace/workspace-username/wrapper_log.txt", "a") as f:
#     f.write(f"{sys.argv}  --  {options}\n")

# unioned_options = set()
# for line in lines:
#     unioned_options = unioned_options.union(convert_args_to_options(json.loads(line)))
# with open(r"/workspace/workspace-username/wrapper_log.txt", "a") as f:
#     f.write(f"unioned_options: {unioned_options}\n")

for line in lines:
    args = json.loads(line)
    _options = convert_args_to_options(args)
    if options.issubset(_options):
        with open(r"{{result_path}}", "w") as f:
            f.write(json.dumps(sys.argv[1:]))
            break
