cleanup() {
    for file in $@; do
	rm $file
    done
}

check_dir() {
    if [ ! -f memory ]; then
	echo "I cannot find memory binary.  Did you..."
	echo "(1) build your assignment?"
	echo "(2) run this script from your assignment directory?"
	return 1
    fi
    return 0
}

