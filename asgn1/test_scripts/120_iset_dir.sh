#!/usr/bin/env bash

## This script performs a simple get request of binary data.


source_dir=`dirname ${BASH_SOURCE}`
source "$source_dir/utils.sh"

if [[ check_dir -eq 1 ]]; then
    exit 1
fi

## To simplify cleanup, add all files that your script uses.
### You can do this ahead of time, or as you create them.
new_files=""
files=`ls .`


## cleanup all of those files now instead of waiting:
cleanup $new_files
new_files=""

## create set command:
echo  "set\ntest/\n0\n" > command
cat ../test_files/antihero.txt >> command
new_files="$new_files command"

## run get command and save to file:
cat command | ./memory > output.txt 2>err.log
mem_rc=$?
new_files="$new_files output.txt err.log"


## Check each of the outputs:
## (1) memory's return code is 1
## (2) memory's stdout is empty
## (3) memory's stderr is the expected value
## (4) no files were added
## If any of these fail, produce a helpful message and set rc=1

rc=0
msg=""

## check if memory's return code is 0
if [[ $mem_rc -eq 0 ]]; then
    msg=$'Memory returned zero error code\n'
    rc=1
fi

if [[ `wc -c < output.txt ` -ne 0 ]]; then
    msg=$"${msg}Memory produced the wrong output to stdout"$'\n'
    rc=1
fi


## check if memory's stderr is the expected value (note: echo adds an
## '\n' automatically)
diff err.log <(echo "Invalid Command") > diff.txt
diff_rc=$?
if [[ $diff_rc -ne 0 ]]; then
    msg=$"${msg}Memory produced the wrong stderr"$'\n'
    rc=1
fi
new_files="$new_files diff.txt"
current_files=`ls`

og_files=`echo "$files $new_files" | xargs -n1 | sort -u | xargs`
after_files=`echo $current_files | xargs -n1 | sort -u | xargs`

diff <(echo $og_files) <(echo $after_files) > file_diff.txt
files_rc=$?
if [[ $files_rc -ne 0 ]]; then
    msg=$"${msg}Memory prodocued extra files"$'\n'
    rc=1
fi
new_files="$new_files file_diff.txt"


## Produce pretty output :-)
if [[ $rc -eq 0 ]]; then
    echo "It worked!"
else
    echo "--------------------------------------------------------------------------------"
    echo "$msg"
    echo "--------------------------------------------------------------------------------"
    echo "return code:"
    echo $mem_rc
    echo "--------------------------------------------------------------------------------"
    echo "stdout:"
    cat output.txt
    echo "--------------------------------------------------------------------------------"
    echo "stderr:"
    cat err.log
    echo "--------------------------------------------------------------------------------"
    if [[ $diff_rc -eq 0 ]]; then
	echo "memory produced correct stderr."
    else
	echo "memory's stderr's difference from correct:"
	cat diff.txt
    fi
    echo "--------------------------------------------------------------------------------"
    if [[ $files_rc -eq 0 ]]; then
	echo "memory output no extra files."
    else
	echo "memory's produced extra files:"
	cat file_diff.txt
    fi
    echo "--------------------------------------------------------------------------------"
fi

cleanup $new_files
exit $rc
