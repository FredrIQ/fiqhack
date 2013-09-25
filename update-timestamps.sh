#!/bin/sh
# Last modified by Alex Smith, 2013-09-25
for x in `git ls-tree -r HEAD | cut -f 2`
do
    sed -i -e "1,2s/Last modified by.*, ....-..-../$(git log -n 1 --pretty=format:'Last modified by %an, %at' $x | perl -pe 's/(\d+)$/`date --date=\@$1 +%F`/e')/" $x
done
