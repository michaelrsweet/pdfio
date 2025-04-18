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

if test $# = 1; then
    echo "Usage: ./test-corpus.sh DIRECTORY"
    exit 1
fi

for file in $(find "$@" -name \*.pdf -print); do
    ./testpdfio $file 2>$file.log || echo $file
done
