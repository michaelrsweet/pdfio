#!/bin/sh
#
# Script to test PDFio against a directory of PDF files.
#
# Copyright © 2025 by Michael R Sweet.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#
# Usage:
#
#   ./test-corpus.sh DIRECTORY
#

if test $# = 0; then
    echo "Usage: ./test-corpus.sh DIRECTORY"
    exit 1
fi

for file in $(find "$@" -name \*.pdf -print); do
    pdfinfo $file >/dev/null 2>&1 || continue;

    ./testpdfio $file >$file.log 2>&1
    if test $? = 0; then
        rm -f $file.log
    else
        echo $file
    fi
done
