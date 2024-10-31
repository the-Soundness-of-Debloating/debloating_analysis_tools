#!/usr/bin/env python3

import argparse


def read_args():
    parser = argparse.ArgumentParser(description="Perform augmentation on a debloated program (by adding back or replacing with exit)")

    parser.add_argument("--lines", dest="lines_list", metavar="LINES_LIST", help="lines to add back (example: '1-2,5-6')")
    parser.add_argument("--remove", action="store_true", dest="is_remove", help="remove lines rather than adding them back")
    parser.add_argument("--exit", action="store_true", dest="is_exit", help="augment by inserting exit")

    parser.add_argument("filepath", metavar="FILEPATH", help="file path to the source file to add back or remove lines (inplace)")
    parser.add_argument("filepath_reference", metavar="REFERENCE_FILEPATH", help="file path to the source file as reference (the original undebloated file)")

    args = parser.parse_args()
    args.lines_list = tuple(args.lines_list.split(",")) if args.lines_list else []

    return args


if __name__ == "__main__":
    args = read_args()
    # read lines from debloated and original file
    with open(args.filepath, "r") as f, open(args.filepath_reference, "r") as fr:
        lines = f.readlines()
        lines_reference = fr.readlines()
    # perform augmentation
    for line_range in args.lines_list:
        if len(line_range) == 0:
            continue
        elif not "-" in line_range:
            print(f"Invalid line range: {line_range}")
            continue
        start, end = line_range.split("-")
        start, end = int(start) - 1, int(end)
        if args.is_exit:
            # insert exit after, and preserve indentation
            main_part = lines[start].lstrip(" \t").rstrip()
            indentation = lines_reference[start].split(lines_reference[start].lstrip(" \t"))[0]
            lines[start] = f'{indentation}{main_part} printf("<This branch (L{start+1}) is removed by Cov debloating tool>\\n"); exit(6);\n'
        elif args.is_remove:
            lines[start:end] = ["\n"] * (end - start)
        else:
            for i in range(start, end):
                # don't replace if trimmed versions are equal (e.g.: "aaa" and "  aaa", preserving indentation in debloated program)
                if lines[i].strip(" \t\n\r").rstrip(";") != lines_reference[i].strip(" \t\n\r").rstrip(";"):
                    lines[i] = lines_reference[i]
    # write lines to debloated file
    with open(args.filepath, "w") as f:
        f.writelines(lines)
