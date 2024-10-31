#!/usr/bin/env python3

import argparse


def read_args():
    parser = argparse.ArgumentParser(description="Replace several lines of code (using the reference source code).")

    parser.add_argument("--lines", dest="lines_list", metavar="LINES_LIST", help="lines to add back (example: '1-2,5-6')")
    parser.add_argument("--remove", action="store_true", dest="is_remove", help="remove lines rather than adding them back")

    parser.add_argument("filepath", metavar="FILEPATH", help="file path to the source file to add back or remove lines (inplace)")
    parser.add_argument("filepath_reference", metavar="REFERENCE_FILEPATH", help="file path to the source file as reference (for add-back, the original undebloated file; for remove, the unmodified debloated file)")

    args = parser.parse_args()
    args.lines_list = tuple(args.lines_list.split(",")) if args.lines_list else []

    return args


if __name__ == "__main__":
    args = read_args()
    # print(args)
    # print(len(args.lines_list))
    # read lines from debloated and original file
    with open(args.filepath, "r") as f, open(args.filepath_reference, "r") as fr:
        lines = f.readlines()
        lines_reference = fr.readlines()
    # perform patching
    for line_range in args.lines_list:
        if "-" in line_range:
            start, end = line_range.split("-")
            start, end = int(start) - 1, int(end)
            # lines[start:end] = lines_reference[start:end] if not args.is_remove else ["\n"] * (end - start)
            for i in range(start, end):
                # if trimmed versions are equal, don't replace (e.g.: don't replace " aaa" by "     aaa")
                if lines[i].strip(" \t\n\r").rstrip(";") != lines_reference[i].strip(" \t\n\r").rstrip(";"):
                    lines[i] = lines_reference[i]
        elif len(line_range):
            line = int(line_range) - 1
            # lines[line] = "\n" if args.is_remove else lines_reference[line]
            if lines[line].strip(" \t\n\r").rstrip(";") != lines_reference[line].strip(" \t\n\r").rstrip(";"):
                lines[line] = lines_reference[line]
    # write lines to debloated file
    with open(args.filepath, "w") as f:
        f.writelines(lines)
