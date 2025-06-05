#!/bin/bash

set -eu

if [ $# -ne 4 ]; then
    echo "Usage: $0 <output> <modulemap> <module_name> <swift_generated_header>"
    exit 1
fi

readonly output="$1"
readonly modulemap="$2"
readonly module_name="$3"
readonly swift_generated_header="$4"

relpath() {
    perl -MFile::Spec -e 'print File::Spec->abs2rel($ARGV[1], $ARGV[0])' "$1" "$2"
}

modulemap_dir=$(perl -MFile::Spec -e 'print File::Spec->rel2abs($ARGV[0])' "${modulemap%/*}")
output_dir=$(perl -MFile::Spec -e 'print File::Spec->rel2abs($ARGV[0])' "${output%/*}")

{
    IFS=
    while read -r line || [ -n "$line" ]; do
        if [[ "$line" =~ ^([[:space:]]*(private[[:space:]]+|textual[[:space:]]+|umbrella[[:space:]]+)*)header[[:space:]]+\"([^\"]+)\"(.*)$ ]]; then
            prefix="${BASH_REMATCH[1]}header "
            header_path="${BASH_REMATCH[3]}"
            suffix="${BASH_REMATCH[4]}"

            original_header_abs_path=$(perl -MFile::Spec -e 'print File::Spec->rel2abs($ARGV[0], $ARGV[1])' "$header_path" "$modulemap_dir")
            new_header_rel_path=$(relpath "$output_dir" "$original_header_abs_path")

            echo "$prefix\"$new_header_rel_path\"$suffix"
        else
            echo "$line"
        fi
    done

    echo ""
    echo "module \"$module_name\".Swift {"
    echo "    header \"$swift_generated_header\""
    echo ""
    echo "    requires objc"
    echo "}"
} < "$modulemap" > "$output"
