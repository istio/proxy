#!/bin/bash

set -e

# Enable memory sanitiser option
export SANITIZE_OPTIONS=-fsanitize=memory
cmake ..
make -B -k
export SANITIZE_OPTIONS=

