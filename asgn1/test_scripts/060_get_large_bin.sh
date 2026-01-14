#!/usr/bin/env bash

# This script performs a simple get command


source_dir=`dirname ${BASH_SOURCE}`
source "$source_dir/utils.sh"

if [[ check_dir -eq 1 ]]; then
    exit 1
fi

## utils includes a cleanup function.  To use it, add all files
## that your script uses. To this variable
new_files=""


## Copy our small text file into the local directory:
cp ../test_files/large_binary.dat test.dat
new_files="$new_files test.dat"


## execute get command, saving memory's output, error, and getting its
## return code
printf "get\ntest.dat\n" | ./memory > output.dat 2>err.log
mem_rc=$?
new_files="$new_files output.dat err.log"


## Check each of the outputs:
## (1) memory's return code is 0
## (2) memory's stdout is the expected file
## If any of these fail, produce a helpful message and set rc=1

### Note: we don't check for stderr being empty; you can use it for
### logging should you like!

rc=0
msg=""

## check if memory's return code is 0
if [[ $mem_rc -ne 0 ]]; then
    msg=$'Memory returned non-zero error code\n'
    rc=1
fi

## check if memory's stdout is the expected file
diff output.dat test.dat > diff.dat
diff_rc=$?
if [[ $diff_rc -ne 0 ]]; then
    msg=$"${msg}Memory produced wrong output"$'\n'
    rc=1
fi
new_files="$new_files diff.dat"


if [[ $rc -eq 0 ]]; then
    echo "It worked!"
else
    echo "--------------------------------------------------------------------------------"
    echo "$msg"
    echo "--------------------------------------------------------------------------------"
    echo "return code:"
    echo $mem_rc
    echo "--------------------------------------------------------------------------------"
    echo "stderr:"
    cat err.log
    echo "--------------------------------------------------------------------------------"
    if [[ $diff_rc -eq 0 ]]; then
	echo "memory produced correct stdout."
    else
	echo "memory's stdout's difference from correct:"
	cat diff.dat
    fi
    echo "--------------------------------------------------------------------------------"
fi

cleanup $new_files
exit $rc


