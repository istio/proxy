#!/bin/bash

set -e

# Enable address sanitiser option
export SANITIZE_OPTIONS=-fsanitize=address
cmake ..
make -B
export SANITIZE_OPTIONS=

