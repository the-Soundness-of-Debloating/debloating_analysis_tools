#!/usr/bin/env python3

# This is a template script containing placeholders in the form of {{xxx}}. Replace them before use.
# The script is a wrapper for running a binary, but instead of running the binary, it logs the arguments that would be passed to the binary.

import sys
import json

with open(r"{{deb_spec_file_path}}", "a") as f:
    # quoted_args = [repr(arg) for arg in sys.argv[1:]]
    f.write(json.dumps(sys.argv[1:]) + '\n')
