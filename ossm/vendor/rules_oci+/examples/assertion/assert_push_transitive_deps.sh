#!/usr/bin/env bash

IMAGE_DIR="$1"

for blob in "$IMAGE_DIR/blobs/sha256"/*; do 
    blob_real_path=$(realpath "$blob")
    blob_real_path_relative="${blob_real_path##*bin/}"
    echo "$blob -> $blob_real_path_relative"
    if [[ ! -e "$blob_real_path_relative" ]]; then
        echo "$blob is not present in the sandbox."
        exit 1
    fi 
done
