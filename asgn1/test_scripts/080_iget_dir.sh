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


## Create a new directory
mkdir new_dir


## run get command and save to file:
printf "get\nnew_dir\n" | ./memory > output.txt 2>err.log
mem_rc=$?
new_files="$new_files output.txt err.log"


## Check each of the outputs:
## (1) memory's return code is 1
## (2) memory's stdout is empty
## (3) memory's stderr is the expected value
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
fi

cleanup $new_files
rm -r new_dir
exit $rc
