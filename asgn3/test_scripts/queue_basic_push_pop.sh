#!/usr/bin/env bash

source_dir=`dirname ${BASH_SOURCE}`
source "$source_dir/utils.sh"

check_dir queue.o
rc=$?

if [[ $rc -eq 1 ]]; then
    exit 1
fi

cfile=basic_push_pop.c

cp queue_users/$cfile .
clang -o queue_test queue.o $cfile

./queue_test >output.txt 2>error.txt
rc=$?

new_files="$cfile queue_test output.txt error.txt"

if [[ $rc -eq 0 ]]; then
    echo "It worked!"
else
    echo "--------------------------------------------------------------------------------"
    echo "return code: $rc"
    echo "--------------------------------------------------------------------------------"
    echo "output:"
    cat output.txt
    echo "--------------------------------------------------------------------------------"
    echo "error:"
    cat error.txt
    echo "--------------------------------------------------------------------------------"

fi

cleanup $new_files
exit $rc
