#!/bin/sh
cd `dirname $0`
sed -e '/^#/d' ../libnethack_common/dat/motd.txt | shuf -n 3 | sed -e '/^+/q' | tail -n1 | sed -e 's/^+//'
