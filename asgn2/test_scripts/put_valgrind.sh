#!/usr/bin/env bash

source_dir=`dirname ${BASH_SOURCE}`
source "$source_dir/utils.sh"

if [[ `check_dir` -eq 1 ]]; then
    exit 1
fi

port=`get_port`

if [[ port -eq 1 ]]; then
    exit 1
fi

# Start up server.
valgrind --error-exitcode=1 --log-file=val_log.txt ./httpserver $port > output.txt 2>error.txt &
pid=$!

# Wait until we can connect.
wait_for_listen $port
wait_rc=$?

if [[ $wait_rc -eq 1 ]]; then
    echo "Server didn't listen on $port in time"
    kill -9 $pid
    wait $pid &>/dev/null
    exit 1
fi


# Test input file.
file="../test_files/small_binary.dat"
infile="temp.txt"
outfile="outtemp.txt"
new_files="$infile $outfile output.txt error.txt val_log.txt"

rc=0
msg=""

files=1000
new_files="$new_files diff.txt msgbody_diff.txt"
for i in $(seq 0 $(($files+10))); do

    echo "This should be overwritten by the text!!" > $infile

    # run the test!
    actual=$(curl -s -w "%{http_code}" -o $outfile localhost:$port/$infile -T $file -H "Expect:")

    # Expected status code.
    expected=200

    # Check the status code.
    if [[ $actual -ne $expected ]]; then
	rc=1
	msg=$'Incorrect status code\n'
	break
    fi

    # Check the file that is saved.
    diff $file $infile  > diff.txt
    diff_rc=$?
    if [[ $diff_rc -ne 0 ]]; then
	msg=$"${msg}Server produced wrong File"$'\n'
	rc=1
	break
    fi


    # Check the file that is saved.
    diff $outfile <(echo "OK")  > msgbody_diff.txt
    msgbody_diff_rc=$?
    if [[ $diff_rc -ne 0 ]]; then
	msg=$"${msg}Server produced wrong message body"$'\n'
	rc=1
	break
    fi
done

## Make sure the server is dead.
kill -9 $pid
wait $pid &>/dev/null

if [[ `wc -l < val_log.txt` -gt 7 ]]; then
    msg=$"${msg}Server died before we killed it (valgrind detected an error)"$'\n'
    rc=1
fi


if [[ $rc -eq 0 ]]; then
    echo "It worked!"
else
    echo "--------------------------------------------------------------------------------"
    echo "$msg"
    echo "--------------------------------------------------------------------------------"
    echo "Your status code: $actual (expected: $expected)"
    echo "--------------------------------------------------------------------------------"
    echo "stdout:"
    cat output.txt
    echo "--------------------------------------------------------------------------------"
    echo "stderr:"
    cat error.txt
    echo "--------------------------------------------------------------------------------"
    if [[ $diff_rc -eq 0 ]]; then
	echo "File is correct."
    else
	echo "File is incorrect.  The difference from correct is:"
	cat diff.txt
    fi
    echo "--------------------------------------------------------------------------------"
    if [[ $msgbody_diff_rc -eq 0 ]]; then
	echo "Message Body is correct."
    else
	echo "Message Body is incorrect.  The difference from correct is:"
	cat msgbody_diff.txt
    fi
    echo "--------------------------------------------------------------------------------"
    echo "valgrind's output:"
    cat val_log.txt
    echo "--------------------------------------------------------------------------------"

fi

# Clean up.

## remove ded files
cleanup $new_files

exit $rc
