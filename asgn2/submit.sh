#!/usr/bin/env bash

set -euo pipefail

python3 -m autograder.run.submit httpserver.c Makefile README.md *.h *.a
