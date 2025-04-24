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
    # Don't worry about test files containing MIME garbage...
    (head -4 $file | grep -q Content-Type) && continue;

    # Or test files containing MacBinary garbage...
    (file $file | grep -q MacBinary) && continue;

    # Don't worry about test files that Xpdf can't handle...
    pdfinfo $file >/dev/null 2>&1 || continue;

    # Run testpdfio to test loading the file...
    ./testpdfio $file >$file.log 2>&1
    if test $? = 0; then
    	# Passed
        rm -f $file.log
    else
    	# Failed, preserve log and write filename to stdout...
        echo $file
    fi
done
