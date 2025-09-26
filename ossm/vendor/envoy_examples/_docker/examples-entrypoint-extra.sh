#!/usr/bin/env bash

set -e -o pipefail

echo "common:examples --sandbox_writable_path=/home/envoybuild" >> repo.bazelrc
echo "common:examples --sandbox_writable_path=/home/envoybuild/.docker" >> repo.bazelrc
