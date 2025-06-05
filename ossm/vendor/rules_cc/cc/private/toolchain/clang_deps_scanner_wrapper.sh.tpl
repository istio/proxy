#!/usr/bin/env bash
#
# Ship the environment to the C++ action
#
set -eu

# Set-up the environment
%{env}

# Call the C++ compiler
%{deps_scanner} -format=p1689 -- %{cc} "$@" >"$DEPS_SCANNER_OUTPUT_FILE"
