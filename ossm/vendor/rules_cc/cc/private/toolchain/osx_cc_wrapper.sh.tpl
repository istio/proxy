#!/bin/sh
#
# Copyright 2015 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# OS X relpath is not really working. This is a wrapper script around gcc
# to simulate relpath behavior.
#
# This wrapper uses install_name_tool to replace all paths in the binary
# (bazel-out/.../path/to/original/library.so) by the paths relative to
# the binary. It parses the command line to behave as rpath is supposed
# to work.
#
# See https://blogs.oracle.com/dipol/entry/dynamic_libraries_rpath_and_mac
# on how to set those paths for Mach-O binaries.
#
set -eu

LIBS=
LIB_PATHS=
LIB_DIRS=
RPATHS=
OUTPUT=

XCRUN=/usr/bin/xcrun
[ -x "$XCRUN" ] || XCRUN="xcrun"

DIRNAME=/usr/bin/dirname
[ -x "$DIRNAME" ] || DIRNAME="dirname"

BASENAME=/usr/bin/basename
[ -x "$BASENAME" ] || BASENAME="basename"

READLINK=/usr/bin/readlink
[ -x "$READLINK" ] || READLINK="readlink"

SED=/usr/bin/sed
[ -x "$SED" ] || SED="sed"

parse_option() {
    opt=$1
    if [ "$OUTPUT" = "1" ]; then
        OUTPUT=$opt
    elif [ "${opt#-l}" != "$opt" ]; then
        LIBS="${opt#-l} $LIBS"
    elif [ "${opt%.so}" != "$opt" ]; then
        LIB_PATHS="$opt $LIB_PATHS"
    elif [ "${opt%.dylib}" != "$opt" ]; then
        LIB_PATHS="$opt $LIB_PATHS"
    elif [ "${opt#-L}" != "$opt" ]; then
        LIB_DIRS="${opt#-L} $LIB_DIRS"
    elif [ "${opt#@loader_path/}" != "$opt" ]; then
        RPATHS="${opt#@loader_path/} $RPATHS"
    elif [ "$opt" = "-o" ]; then
        # output is coming
        OUTPUT=1
    fi
}

# let parse the option list
for i in "$@"; do
    case $i in
        @*)
            file=${i#@}
            if [ -r "$file" ]; then
                while IFS= read -r opt; do
                    parse_option "$opt"
                done < "$file" || exit 1
            fi
            ;;
        *)
            parse_option "$i"
            ;;
    esac
done

# Set-up the environment
%{env}

# Call the C++ compiler
%{cc} "$@"

# Generate an empty file if header processing succeeded.
case $OUTPUT in
    *.h.processed)
        : > "$OUTPUT"
        ;;
esac

get_library_path() {
    lib=$1
    for libdir in $LIB_DIRS; do
        if [ -f "$libdir/lib$lib.so" ]; then
            echo "$libdir/lib$lib.so"
            return
        elif [ -f "$libdir/lib$lib.dylib" ]; then
            echo "$libdir/lib$lib.dylib"
            return
        fi
    done
}

# A convenient method to return the actual path even for non symlinks
# and multi-level symlinks.
get_realpath() {
    previous=$1
    next=$($READLINK "$previous" 2>/dev/null || true)
    while [ -n "$next" ]; do
        previous=$next
        next=$($READLINK "$previous" 2>/dev/null || true)
    done
    echo "$previous"
}

# Get the path of a lib inside a tool
get_otool_path() {
    # the lib path is the path of the original lib relative to the workspace
    get_realpath "$1" | $SED 's|^.*/bazel-out/|bazel-out/|'
}

call_install_name() {
    $XCRUN install_name_tool -change "$(get_otool_path "$1")" \
        "@loader_path/$2/$3" "$OUTPUT"
}

# Do replacements in the output
for rpath in $RPATHS; do
    for lib in $LIBS; do
        libname=
        if [ -f "$($DIRNAME "$OUTPUT")/$rpath/lib$lib.so" ]; then
            libname="lib$lib.so"
        elif [ -f "$($DIRNAME "$OUTPUT")/$rpath/lib$lib.dylib" ]; then
            libname="lib$lib.dylib"
        fi
        # ${libname-} --> return $libname if defined, or undefined otherwise. This is to make
        # this set -e friendly
        if [ -n "$libname" ]; then
            libpath=$(get_library_path "$lib")
            if [ -n "$libpath" ]; then
                call_install_name "$libpath" "$rpath" "$libname"
            fi
        fi
    done
    for libpath in $LIB_PATHS; do
        if [ -f "$libpath" ]; then
            libname=$($BASENAME "$libpath")
            if [ -f "$($DIRNAME "$OUTPUT")/$rpath/$libname" ]; then
                call_install_name "$libpath" "$rpath" "$libname"
            fi
        fi
    done
done
