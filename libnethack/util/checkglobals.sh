#!/bin/sh
# Last modified by Alex Smith, 2013-10-16
for x in `nm "$1" | egrep -v '^[0-9a-f]* *([tTrUwA]|d __)' | sort -k1.12 | cut -c12-`
do
    echo "warning: found static or global variable $x" 1>&2
done
