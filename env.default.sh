#!/bin/bash

# This is a default env file (which works in my environment).
# Copy this file to "env.sh" and modify the variables to fit your environment.


if [[ "$(basename $0)" == "env.default.sh" && -f env.sh ]]; then
    source env.sh
    exit 0
fi

script_dir=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
export ANALYSIS_TOOLS_DIR="$script_dir"

workspace_dir="/workspace"
export DEBAUG_DIR="$workspace_dir/debaug-deb-chiselbench"
export CRASH_REPAIR_DIR="$workspace_dir/CrashRepair"
export DEBLOATING_STUDY_DIR="$workspace_dir/debloating_study"
export DEB_VUL_REPAIR_DIR="$workspace_dir/deb-vul-repair"
