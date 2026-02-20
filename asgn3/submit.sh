#!/usr/bin/env bash

set -euo pipefail

python3 -m autograder.run.submit queue.c rwlock.c Makefile README.md
