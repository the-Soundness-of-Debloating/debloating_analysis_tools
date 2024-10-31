#!/usr/bin/env python3

# Example:
#     Usage: python align_source_codes.py ./date-debloated.c ./date-original.c
#     Output File: date-debloated.aligned.c date-original.aligned.c

# Test Cases (check manually):
#   - line 1995 program rm
#   - line 1997 program rm
#   - line 4870-5000 program rm
#   - "free((void *)tmp);" program rm


import argparse
import subprocess
from pathlib import Path
from difflib import SequenceMatcher
from typing import List, Callable


class AlignedRange:
    __slots__ = ["start_1", "start_2", "size"]
    def __init__(self, start_1, start_2, size):
        self.start_1 = start_1
        self.start_2 = start_2
        self.size = size
    def __str__(self):
        return f"({self.start_1}, {self.start_2}, {self.size})"


def read_args():
    parser = argparse.ArgumentParser(description='Align the debloated and original version of source codes.')
    parser.add_argument("filepath1", metavar="filepath1",
                        help='file path to debloated or original source code')
    parser.add_argument("filepath2", metavar="filepath2",
                        help='file path to debloated or original source code')
    parser.add_argument("--no-reformat", action='store_true', dest="no_reformat",
                        help="align without reformatting (used when source codes are not grammatically correct)")
    return parser.parse_args()


def get_file_lines(filepath: str, no_reformat: bool):
    if no_reformat:
        with open(filepath) as f:
            return [s.strip("\r\n") for s in f.readlines()]
    else:
        # reformat two source files to facilitate alignment
        # note that only after version 14 that clang-format supports "-style=file:path" as argument
        # that's why this tool currently does not support using file as format config
        format_style = "{BasedOnStyle: llvm, BreakBeforeBraces: Allman, ColumnLimit: 10000, AllowShortFunctionsOnASingleLine: None}"
        # https://stackoverflow.com/questions/43022687/python-subprocess-argument-with-equal-sign-and-space
        reformat = subprocess.run(["clang-format-15", f'-style={format_style}', filepath], capture_output=True, text=True)
        return reformat.stdout.splitlines()


def strip_code_line(s: str):
    # There are special cases where the wrapping brackets are removed and the indentation is altered.
    # We need to take them into consideration and only compare the properly trimmed lines.
    s = s.strip(" \t;")
    if s.startswith("{") and s.endswith("}"):
        s = s.lstrip(" \t{").rstrip(" \t};")
    return s


def align_two_sequences(seq_1: List[str], seq_2: List[str],
                        previously_aligned_lines: List[AlignedRange],
                        preprocess_code_line: Callable[[str], str],
                        isjunk: Callable[[str], bool]):
    new_aligned_lines = []  # line index starts from 0
    aligned_seq_1, aligned_seq_2 = [], []
    next_line_to_handle_1, next_line_to_handle_2 = 0, 0  # line index starts from 0

    # preprocess the code lines to facilitate code alignment
    seq_1_preprocessed, seq_2_preprocessed = seq_1, seq_2
    if preprocess_code_line:
        seq_1_preprocessed = [preprocess_code_line(s) for s in seq_1]
        seq_2_preprocessed = [preprocess_code_line(s) for s in seq_2]

    # add empty element to the back of `aligned_lines_1` to simplify the loop below
    if (len(previously_aligned_lines) == 0 or previously_aligned_lines[-1].size != 0):
        previously_aligned_lines.append(AlignedRange(len(seq_1), len(seq_2), 0))

    for i in range(len(previously_aligned_lines)):
        # handle previously unaligned lines
        # 1 match
        _seq_1 = seq_1_preprocessed[next_line_to_handle_1:previously_aligned_lines[i].start_1]
        _seq_2 = seq_2_preprocessed[next_line_to_handle_2:previously_aligned_lines[i].start_2]
        start_1, start_2 = next_line_to_handle_1, next_line_to_handle_2
        matcher = SequenceMatcher(isjunk, _seq_1, _seq_2, False)  # do not align junk lines
        matching_blocks = matcher.get_matching_blocks()  # List[Match]

        # 2 align
        # There will always be an empty matching_block at the back. No need to worry about the last loop.
        for matching_block in matching_blocks:
            # append unaligned lines
            aligned_seq_1 += seq_1[next_line_to_handle_1:start_1+matching_block.a]
            aligned_seq_2 += seq_2[next_line_to_handle_2:start_2+matching_block.b]
            # add newline paddings
            aligned_seq_1 += [""] * (len(aligned_seq_2) - len(aligned_seq_1))
            aligned_seq_2 += [""] * (len(aligned_seq_1) - len(aligned_seq_2))
            # record new aligned lines
            if matching_block.size > 0:
                new_aligned_lines.append(AlignedRange(len(aligned_seq_1), len(aligned_seq_2),
                                                      matching_block.size))
                aligned_seq_1 += seq_1[start_1+matching_block.a:start_1+matching_block.a+matching_block.size]
                aligned_seq_2 += seq_2[start_2+matching_block.b:start_2+matching_block.b+matching_block.size]
            # update loop variables
            next_line_to_handle_1 = start_1 + matching_block.a + matching_block.size
            next_line_to_handle_2 = start_2 + matching_block.b + matching_block.size

        # record previously aligned lines
        if previously_aligned_lines[i].size > 0:
            new_aligned_lines.append(AlignedRange(len(aligned_seq_1), len(aligned_seq_2),
                                                  previously_aligned_lines[i].size))
            aligned_seq_1 += seq_1[previously_aligned_lines[i].start_1
                                   :previously_aligned_lines[i].start_1+previously_aligned_lines[i].size]
            aligned_seq_2 += seq_2[previously_aligned_lines[i].start_2
                                   :previously_aligned_lines[i].start_2+previously_aligned_lines[i].size]
            next_line_to_handle_1 += previously_aligned_lines[i].size
            next_line_to_handle_2 += previously_aligned_lines[i].size

    return aligned_seq_1, aligned_seq_2, new_aligned_lines


def main():
    args = read_args()

    seq_1, seq_2 = get_file_lines(args.filepath1, args.no_reformat), get_file_lines(args.filepath2, args.no_reformat)

    # Suppose we need to compare "...agggggbcda..." with "...bcdaggggg...".
    # In our scenario to compare original sequence with the debloated sequence, where the sequence
    #   is debloated but not rearranged, it is obviously better to align the "bcda" part than aligning
    #   the longer "aggggg" part (the latter is supposedly better in general scenarios).
    # Therefore, the debloated version should be used as the first argument of SequenceMatcher (difflib loops
    #   over every line in the first sequence and compare it to the second sequence).
    get_output_filepath = lambda fn: f"{Path(fn).stem}.aligned{Path(fn).suffix}" # output to current dir
    if len(seq_1) <= len(seq_2):
        filename_debloated_output, seq_debloated = get_output_filepath(args.filepath1), seq_1
        filename_original_output, seq_original = get_output_filepath(args.filepath2), seq_2
    else:
        filename_debloated_output, seq_debloated = get_output_filepath(args.filepath2), seq_2
        filename_original_output, seq_original = get_output_filepath(args.filepath1), seq_1

    # print(f"Align source codes '{args.filepath1}' and '{args.filepath2}'.")

    aligned_seq_debloated, aligned_seq_original, aligned_lines = seq_debloated, seq_original, []
    # first alignment - only align top-level declarations such as function declaration lines
    aligned_seq_debloated, aligned_seq_original, aligned_lines = align_two_sequences(
        aligned_seq_debloated, aligned_seq_original, aligned_lines,
        None, lambda s: len(s) <= 1 or s.startswith(("\t", " ")))
    # second alignment - leave out lines that only contain a single curly brace
    aligned_seq_debloated, aligned_seq_original, aligned_lines = align_two_sequences(
        aligned_seq_debloated, aligned_seq_original, aligned_lines,
        strip_code_line, lambda s: len(s) <= 1)
    # third alignment - align lines that only contain a single curly brace
    aligned_seq_debloated, aligned_seq_original, aligned_lines = align_two_sequences(
        aligned_seq_debloated, aligned_seq_original, aligned_lines,
        strip_code_line, lambda s: len(s) == 0)

    # record unaligned lines
    next_unaligned_line = 0
    unaligned_lines = []
    for aligned_range in aligned_lines:
        for i in range(next_unaligned_line, aligned_range.start_1):
            if len(strip_code_line(aligned_seq_debloated[i])):
                unaligned_lines.append(i + 1)
        next_unaligned_line = aligned_range.start_1 + aligned_range.size

    # remove useless empty lines (TODO: this is definitely not a good solution)
    new_aligned_seq_debloated, new_aligned_seq_original = [], []
    unaligned_lines_shift = [0] * len(unaligned_lines)
    aligned_seq_debloated += [""] * (len(aligned_seq_original) - len(aligned_seq_debloated))
    aligned_seq_original += [""] * (len(aligned_seq_debloated) - len(aligned_seq_original))
    for i, v in enumerate(zip(aligned_seq_debloated, aligned_seq_original)):
        if len(v[0].strip()) == 0 and len(v[1].strip()) == 0:
            # decrease by one if line number is larger than the current one
            # cannot perform the decrease here because the (i + 1) is the unshifted line number
            for j in range(len(unaligned_lines)):
                if unaligned_lines[j] > i + 1:
                    unaligned_lines_shift[j] += 1
        else:
            new_aligned_seq_debloated.append(v[0])
            new_aligned_seq_original.append(v[1])
    unaligned_lines = [(v - unaligned_lines_shift[i]) for i, v in enumerate(unaligned_lines)]

    # print unaligned lines
    print("Lines in debloated program that failed to align:",
        f"{', '.join(map(str, unaligned_lines))}", f"({len(unaligned_lines)} lines in total)")

    with open(filename_debloated_output, "w") as fd, open(filename_original_output, "w") as fo:
        print(f"Output aligned source files to '{filename_debloated_output}' and '{filename_original_output}'.")
        fd.write("\n".join(new_aligned_seq_debloated))
        fo.write("\n".join(new_aligned_seq_original))


if __name__ == "__main__":
    main()
