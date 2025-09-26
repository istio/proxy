#!/bin/bash

set -e

# Enable thread sanitiser option
export SANITIZE_OPTIONS=-fsanitize=thread
cmake ..
make -B
export SANITIZE_OPTIONS=

