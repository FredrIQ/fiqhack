#!/bin/sh
# Last modified by Alex Smith, 2015-07-20
# Copyright (C) 2013, 2015 Alex Smith
# NetHack may be freely redistributed. See license for details.
for x in `nm "$1" | egrep -v '^[0-9a-f]* *[tTrUwAd]' | sort -k1.20 | cut -c20-`
do
    echo "warning: found static or global variable $x" 1>&2
done
