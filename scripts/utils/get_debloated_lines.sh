#!/bin/bash

if [ -z $1 ] || [ -z $2 ] || [ -z $3 ]; then echo "Missing arguments!" && exit 1; fi
DEB_SRC=$1
ORI_SRC=$2
OUTPUT_FILE=$3

# This solution will view lines that are different in the debloated and original source code as debloated lines.
# (such as unaligned "}"s, which will cause compilation errors after adding back)
# Use python to implement it instead.

echo -n " " > $OUTPUT_FILE
sed 's/^[ \t]*//;s/[ \t;]*$//' $DEB_SRC > $DEB_SRC.tmp.trimmed.c
sed 's/^[ \t]*//;s/[ \t;]*$//' $ORI_SRC > $ORI_SRC.tmp.trimmed.c
diff --unchanged-line-format= --new-line-format="%dn%c' '" --old-line-format= \
    <(nl -b a $ORI_SRC.tmp.trimmed.c) <(nl -b a $DEB_SRC.tmp.trimmed.c) >> $OUTPUT_FILE
rm $DEB_SRC.tmp.trimmed.c $ORI_SRC.tmp.trimmed.c
echo Output debloated lines to file \'$OUTPUT_FILE\'.
