#!/bin/sh
# Last modified by Alex Smith, 2013-11-27

# Generate a TAGS file for use with Emacs.
# Run from the root of the distribution, not the scripts folder.
etags */*.[ch] */*/*.[ch] */*/*/*.[ch]
