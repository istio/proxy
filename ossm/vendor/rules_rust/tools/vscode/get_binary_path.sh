#!/bin/bash

# Check if an argument was provided
if [ $# -eq 0 ]; then
    echo "Usage: $0 <path>" >&2
    exit 1
fi

# Get the input path
input_path="$1"

# Convert to absolute path if relative
if [[ "$input_path" != /* ]]; then
    # Extract directory and filename using parameter expansion
    dir="${input_path%/*}"
    file="${input_path##*/}"

    # Handle case where path has no directory component
    if [[ "$dir" == "$input_path" ]]; then
        dir="."
    fi

    input_path="$(cd "$dir" && pwd)/$file"
fi

# Check if the path contains "bazel-out"
if [[ "$input_path" == *"bazel-out"* ]]; then
    # Extract everything from "bazel-out" onwards
    result="${input_path#*bazel-out}"
    full_path="bazel-out${result}"
    
    # Strip .runfiles wrapper path if present to get actual binary
    # bazel-out/.../bin/foo/bar.runfiles/_main/foo/bar -> bazel-out/.../bin/foo/bar
    if [[ "$full_path" == *".runfiles/"* ]]; then
        full_path="${full_path%.runfiles/*}"
    fi
    
    echo "$full_path" >&2
else
    echo "Error: Path does not contain 'bazel-out'" >&2
    exit 1
fi
