#!/usr/bin/env python3

import argparse


# Note:
#   This python version is only used in cov_fix tool.
#   Most of the times, you should use the bash version instead.
#   Even in cov_fix tool, if the source code is properly aligned, you should use the bash version.


# bash version:
#   This solution will view lines that are different in the debloated and original source code as debloated lines.
#   (such as unaligned "}"s, which will cause compilation errors after adding back)
#   Use python to implement it instead.
# # echo -n " " > $OUTPUT_FILE
# # sed 's/^[ \t]*//;s/[ \t;]*$//' $DEB_SRC > $DEB_SRC.tmp.trimmed.c
# # sed 's/^[ \t]*//;s/[ \t;]*$//' $ORI_SRC > $ORI_SRC.tmp.trimmed.c
# # diff --unchanged-line-format= --new-line-format="%dn%c' '" --old-line-format= \
# #     <(nl -b a $ORI_SRC.tmp.trimmed.c) <(nl -b a $DEB_SRC.tmp.trimmed.c) >> $OUTPUT_FILE
# # rm $DEB_SRC.tmp.trimmed.c $ORI_SRC.tmp.trimmed.c
# # echo Output debloated lines to file \'$OUTPUT_FILE\'.


def read_args():
    parser = argparse.ArgumentParser(description="Find out all lines that get debloated.")

    parser.add_argument("-o", "--output", metavar="OUTPUT_FILEPATH", default="debloated-lines.txt",
                        help="file path to the output file (default: debloated-lines.txt)", dest="filepath_output")

    parser.add_argument("filepath_debloated", metavar="DEBLOATED_FILEPATH", help="file path to the debloated source file (aligned)")
    parser.add_argument("filepath_original", metavar="ORIGINAL_FILEPATH", help="file path to the original source file (aligned)")

    return parser.parse_args()


if __name__ == "__main__":
    args = read_args()
    # read lines from debloated and original file
    with open(args.filepath_debloated, "r") as fd, open(args.filepath_original, "r") as fo:
        lines_debloated = fd.readlines()
        lines_original = fo.readlines()
    # get debloated lines by comparing the two files
    output = []
    for i in range(len(lines_debloated)):
        # also strip ";" to avoid aligning it with a normal statement erroneously
        strip_d = lines_debloated[i].strip().rstrip(";")
        strip_o = lines_original[i].strip().rstrip(";")
        # `if strip_d != strip_o and len(strip_d) == 0:` is incorrect!
        # "if {A} else {B}" --deb--> "{B}" --cov-fix--> "if { {B}"
        if strip_d != strip_o:
            output.append(i + 1)
    # output to file
    print(f"Output debloated lines to file '{args.filepath_output}'.")
    with open(args.filepath_output, "w") as f:
        f.write(" " + " ".join(map(str, output)))
