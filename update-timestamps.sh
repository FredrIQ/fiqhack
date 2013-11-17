#!/bin/sh
# Last modified by Sean Hunt, 2013-11-16
STASH=$(git diff | ifne -n echo false | ifne echo true)
$STASH && git stash save --keep-index -q
for x in $(git diff --name-only --diff-filter=M --cached)
do
    sed -i -e "1,2s/Last modified by.*, ....-..-../Last modified by $(git config user.name), $(date -Idate)/" $x
    git add $x
done
$STASH && git stash pop -q
