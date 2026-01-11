#!/bin/sh
#
# Script to test PDFio against a directory of PDF files.
#
# Copyright © 2025-2026 by Michael R Sweet.
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

if (echo "testing\c"; echo 1,2,3) | grep c >/dev/null; then
        ac_n=-n
        ac_c=
else
        ac_n=
        ac_c='\c'
fi

for file in $(find "$@" -name \*.pdf -print); do
    # Run testpdfio to test loading the file...
    echo $ac_n "\r$file$ac_c"
    ./testpdfio $file >$file.log 2>&1
    if test $? = 0; then
    	# Passed
        echo $ac_n "\r                                                              $ac_c"
        rm -f $file.log
    else
    	# Failed, preserve log and write filename to stdout...
    	echo ""
    fi
done
