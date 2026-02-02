#!/usr/bin/env bash

set -euo pipefail

python3 -m  autograder.run.submit memory.c Makefile README.md --allow-late
