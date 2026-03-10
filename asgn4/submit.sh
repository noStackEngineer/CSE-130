#!/usr/bin/env bash

set -euo pipefail

python3 -m autograder.run.submit httpserver.c lockmap.h lockmap.c Makefile README.md
