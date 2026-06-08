#!/usr/bin/env bash

# This bash script is a wrapper around the NodeJS JavaScript file
# entry point with the following bazel label:
#     {{entry_point_label}}
#
# The script's was generated to execute the js_binary target
#     {{target_label}}
#
# The template used to generate this script is
#     {{template_label}}

set -o pipefail -o errexit -o nounset

{{envs}}

# ==============================================================================
# Prepare stdout capture, stderr capture && logging
# ==============================================================================

if [ "${JS_BINARY__STDOUT_OUTPUT_FILE:-}" ] || [ "${JS_BINARY__SILENT_ON_SUCCESS:-}" ]; then
    STDOUT_CAPTURE=$(mktemp)
fi

if [ "${JS_BINARY__STDERR_OUTPUT_FILE:-}" ] || [ "${JS_BINARY__SILENT_ON_SUCCESS:-}" ]; then
    STDERR_CAPTURE=$(mktemp)
fi

export JS_BINARY__LOG_PREFIX="{{log_prefix_rule_set}}[{{log_prefix_rule}}]"

function logf_stderr {
    local format_string="$1\n"
    shift
    if [ "${STDERR_CAPTURE:-}" ]; then
        # shellcheck disable=SC2059,SC2046
        echo -e $(printf "$format_string" "$@") >>"$STDERR_CAPTURE"
    else
        # shellcheck disable=SC2059,SC2046
        echo -e $(printf "$format_string" "$@") >&2
    fi
}

function logf_fatal {
    if [ "${JS_BINARY__LOG_FATAL:-}" ]; then
        if [ "${STDERR_CAPTURE:-}" ]; then
            printf "FATAL: %s: " "$JS_BINARY__LOG_PREFIX" >>"$STDERR_CAPTURE"
        else
            printf "FATAL: %s: " "$JS_BINARY__LOG_PREFIX" >&2
        fi
        logf_stderr "$@"
    fi
}

function logf_error {
    if [ "${JS_BINARY__LOG_ERROR:-}" ]; then
        if [ "${STDERR_CAPTURE:-}" ]; then
            printf "ERROR: %s: " "$JS_BINARY__LOG_PREFIX" >>"$STDERR_CAPTURE"
        else
            printf "ERROR: %s: " "$JS_BINARY__LOG_PREFIX" >&2
        fi
        logf_stderr "$@"
    fi
}

function logf_warn {
    if [ "${JS_BINARY__LOG_WARN:-}" ]; then
        if [ "${STDERR_CAPTURE:-}" ]; then
            printf "WARN: %s: " "$JS_BINARY__LOG_PREFIX" >>"$STDERR_CAPTURE"
        else
            printf "WARN: %s: " "$JS_BINARY__LOG_PREFIX" >&2
        fi
        logf_stderr "$@"
    fi
}

function logf_info {
    if [ "${JS_BINARY__LOG_INFO:-}" ]; then
        if [ "${STDERR_CAPTURE:-}" ]; then
            printf "INFO: %s: " "$JS_BINARY__LOG_PREFIX" >>"$STDERR_CAPTURE"
        else
            printf "INFO: %s: " "$JS_BINARY__LOG_PREFIX" >&2
        fi
        logf_stderr "$@"
    fi
}

function logf_debug {
    if [ "${JS_BINARY__LOG_DEBUG:-}" ]; then
        if [ "${STDERR_CAPTURE:-}" ]; then
            printf "DEBUG: %s: " "$JS_BINARY__LOG_PREFIX" >>"$STDERR_CAPTURE"
        else
            printf "DEBUG: %s: " "$JS_BINARY__LOG_PREFIX" >&2
        fi
        logf_stderr "$@"
    fi
}

function resolve_execroot_bin_path {
    local short_path="$1"
    if [[ "$short_path" == ../* ]]; then
        echo "$JS_BINARY__EXECROOT/${BAZEL_BINDIR:-$JS_BINARY__BINDIR}/external/${short_path:3}"
    else
        echo "$JS_BINARY__EXECROOT/${BAZEL_BINDIR:-$JS_BINARY__BINDIR}/$short_path"
    fi
}

function resolve_execroot_src_path {
    local short_path="$1"
    if [[ "$short_path" == ../* ]]; then
        echo "$JS_BINARY__EXECROOT/external/${short_path:3}"
    else
        echo "$JS_BINARY__EXECROOT/$short_path"
    fi
}

_exit() {
    EXIT_CODE=$?

    if [ "${STDERR_CAPTURE:-}" ]; then
        if [ "${JS_BINARY__STDERR_OUTPUT_FILE:-}" ]; then
            cp -f "$STDERR_CAPTURE" "$JS_BINARY__STDERR_OUTPUT_FILE"
        fi
        if [ "$EXIT_CODE" != 0 ] || [ -z "${JS_BINARY__SILENT_ON_SUCCESS:-}" ]; then
            cat "$STDERR_CAPTURE" >&2
        fi
        rm "$STDERR_CAPTURE"
    fi

    if [ "${STDOUT_CAPTURE:-}" ]; then
        if [ "${JS_BINARY__STDOUT_OUTPUT_FILE:-}" ]; then
            cp -f "$STDOUT_CAPTURE" "$JS_BINARY__STDOUT_OUTPUT_FILE"
        fi
        if [ "$EXIT_CODE" != 0 ] || [ -z "${JS_BINARY__SILENT_ON_SUCCESS:-}" ]; then
            cat "$STDOUT_CAPTURE"
        fi
        rm "$STDOUT_CAPTURE"
    fi

    logf_debug "exit code: %s" "$EXIT_CODE"

    exit $EXIT_CODE
}

trap _exit EXIT

# ==============================================================================
# Initialize RUNFILES environment variable
# ==============================================================================
{{initialize_runfiles}}
# TODO(2.0): export only JS_BINARY__RUNFILES
export RUNFILES
JS_BINARY__RUNFILES="$RUNFILES"
export JS_BINARY__RUNFILES

# ==============================================================================
# Prepare to run main program
# ==============================================================================

# Convert stdout, stderr and exit_code capture outputs paths to absolute paths
if [ "${JS_BINARY__STDOUT_OUTPUT_FILE:-}" ]; then
    JS_BINARY__STDOUT_OUTPUT_FILE="$PWD/$JS_BINARY__STDOUT_OUTPUT_FILE"
fi
if [ "${JS_BINARY__STDERR_OUTPUT_FILE:-}" ]; then
    JS_BINARY__STDERR_OUTPUT_FILE="$PWD/$JS_BINARY__STDERR_OUTPUT_FILE"
fi
if [ "${JS_BINARY__EXIT_CODE_OUTPUT_FILE:-}" ]; then
    JS_BINARY__EXIT_CODE_OUTPUT_FILE="$PWD/$JS_BINARY__EXIT_CODE_OUTPUT_FILE"
fi

if [[ "$PWD" == *"/bazel-out/"* ]]; then
    bazel_out_segment="/bazel-out/"
elif [[ "$PWD" == *"/BAZEL-~1/"* ]]; then
    bazel_out_segment="/BAZEL-~1/"
elif [[ "$PWD" == *"/bazel-~1/"* ]]; then
    bazel_out_segment="/bazel-~1/"
fi

if [[ "${bazel_out_segment:-}" ]]; then
    if [ "${JS_BINARY__USE_EXECROOT_ENTRY_POINT:-}" ] && [ "${JS_BINARY__EXECROOT:-}" ]; then
        logf_debug "inheriting JS_BINARY__EXECROOT %s from parent js_binary process as JS_BINARY__USE_EXECROOT_ENTRY_POINT is set" "$JS_BINARY__EXECROOT"
    else
        # We in runfiles and we don't yet know the execroot
        rest="${PWD#*"$bazel_out_segment"}"
        index=$((${#PWD} - ${#rest} - ${#bazel_out_segment}))
        if [ ${index} -lt 0 ]; then
            printf "\nERROR: %s: No 'bazel-out' folder found in path '${PWD}'\n" "$JS_BINARY__LOG_PREFIX" >&2
            exit 1
        fi
        JS_BINARY__EXECROOT="${PWD:0:$index}"
    fi
else
    if [ "${JS_BINARY__USE_EXECROOT_ENTRY_POINT:-}" ] && [ "${JS_BINARY__EXECROOT:-}" ]; then
        logf_debug "inheriting JS_BINARY__EXECROOT %s from parent js_binary process as JS_BINARY__USE_EXECROOT_ENTRY_POINT is set" "$JS_BINARY__EXECROOT"
    else
        # We are in execroot or in some other context all together such as a nodejs_image or a manually run js_binary
        JS_BINARY__EXECROOT="$PWD"
    fi

    if [ -z "${JS_BINARY__NO_CD_BINDIR:-}" ]; then
        if [ -z "${BAZEL_BINDIR:-}" ]; then
            logf_fatal "BAZEL_BINDIR must be set in environment to the makevar \$(BINDIR) in js_binary build actions (which \
run in the execroot) so that build actions can change directories to always run out of the root of the Bazel output \
tree. See https://docs.bazel.build/versions/main/be/make-variables.html#predefined_variables. This is automatically set \
by 'js_run_binary' (https://github.com/aspect-build/rules_js/blob/main/docs/js_run_binary.md) which is the recommended \
rule to use for using a js_binary as the tool of a build action. If this is not a build action you can set the \
BAZEL_BINDIR to '.' instead to supress this error. For more context on this design decision, please read the \
aspect_rules_js README https://github.com/aspect-build/rules_js/tree/dbb5af0d2a9a2bb50e4cf4a96dbc582b27567155#running-nodejs-programs."
            exit 1
        fi

        # Since the process was launched in the execroot, we automatically change directory into the root of the
        # output tree (which we expect to be set in BAZEL_BIN). See
        # https://github.com/aspect-build/rules_js/tree/dbb5af0d2a9a2bb50e4cf4a96dbc582b27567155#running-nodejs-programs
        # for more context on why we do this.
        logf_debug "changing directory to BAZEL_BINDIR (root of Bazel output tree) %s" "$BAZEL_BINDIR"
        cd "$BAZEL_BINDIR"
    fi
fi
export JS_BINARY__EXECROOT

if [ "${JS_BINARY__USE_EXECROOT_ENTRY_POINT:-}" ]; then
    if [ -z "${BAZEL_BINDIR:-}" ]; then
        logf_fatal "Expected BAZEL_BINDIR to be set when JS_BINARY__USE_EXECROOT_ENTRY_POINT is set"
        exit 1
    fi
    if [ -z "${JS_BINARY__COPY_DATA_TO_BIN:-}" ] && [ -z "${JS_BINARY__ALLOW_EXECROOT_ENTRY_POINT_WITH_NO_COPY_DATA_TO_BIN:-}" ]; then
        logf_fatal "Expected js_binary copy_data_to_bin to be True when js_run_binary use_execroot_entry_point is True. \
To disable this validation you can set allow_execroot_entry_point_with_no_copy_data_to_bin to True in js_run_binary"
        exit 1
    fi
fi

if [ "${JS_BINARY__NO_RUNFILES:-}" ]; then
    if [ -z "${JS_BINARY__COPY_DATA_TO_BIN:-}" ] && [ -z "${JS_BINARY__ALLOW_EXECROOT_ENTRY_POINT_WITH_NO_COPY_DATA_TO_BIN:-}" ]; then
        logf_fatal "Expected js_binary copy_data_to_bin to be True when js_binary use_execroot_entry_point is True. \
To disable this validation you can set allow_execroot_entry_point_with_no_copy_data_to_bin to True in js_run_binary"
        exit 1
    fi
fi

if [ "${JS_BINARY__USE_EXECROOT_ENTRY_POINT:-}" ] || [ "${JS_BINARY__NO_RUNFILES:-}" ]; then
    entry_point=$(resolve_execroot_bin_path "{{entry_point_path}}")
else
    entry_point="$JS_BINARY__RUNFILES/{{workspace_name}}/{{entry_point_path}}"
fi
if [ ! -f "$entry_point" ]; then
    logf_fatal "the entry_point '%s' not found" "$entry_point"
    exit 1
fi

node="$(_normalize_path "{{node}}")"
if [ "${node:0:1}" = "/" ]; then
    # A user may specify an absolute path to node using target_tool_path in node_toolchain
    export JS_BINARY__NODE_BINARY="$node"
    if [ ! -f "$JS_BINARY__NODE_BINARY" ]; then
        logf_fatal "node binary '%s' not found" "$JS_BINARY__NODE_BINARY"
        exit 1
    fi
else
    if [ "${JS_BINARY__NO_RUNFILES:-}" ]; then
        export JS_BINARY__NODE_BINARY
        JS_BINARY__NODE_BINARY=$(resolve_execroot_src_path "{{node}}")
    else
        export JS_BINARY__NODE_BINARY="$JS_BINARY__RUNFILES/{{workspace_name}}/{{node}}"
    fi
    if [ ! -f "$JS_BINARY__NODE_BINARY" ]; then
        logf_fatal "node binary '%s' not found" "$JS_BINARY__NODE_BINARY"
        exit 1
    fi
fi
if [ "$_IS_WINDOWS" -ne "1" ] && [ ! -x "$JS_BINARY__NODE_BINARY" ]; then
    logf_fatal "node binary '%s' is not executable" "$JS_BINARY__NODE_BINARY"
    exit 1
fi

npm="{{npm}}"
if [ "$npm" ]; then
    npm="$(_normalize_path "$npm")"
    if [ "${npm:0:1}" = "/" ]; then
        # A user may specify an absolute path to npm using npm_path in node_toolchain
        export JS_BINARY__NPM_BINARY="$npm"
        if [ ! -f "$JS_BINARY__NPM_BINARY" ]; then
            logf_fatal "npm binary '%s' not found" "$JS_BINARY__NPM_BINARY"
            exit 1
        fi
    else
        if [ "${JS_BINARY__NO_RUNFILES:-}" ]; then
            export JS_BINARY__NPM_BINARY
            JS_BINARY__NPM_BINARY=$(resolve_execroot_src_path "{{npm}}")
        else
            export JS_BINARY__NPM_BINARY="$JS_BINARY__RUNFILES/{{workspace_name}}/{{npm}}"
        fi
        if [ ! -f "$JS_BINARY__NPM_BINARY" ]; then
            logf_fatal "npm binary '%s' not found" "$JS_BINARY__NPM_BINARY"
            exit 1
        fi
    fi
    if [ "$_IS_WINDOWS" -ne "1" ] && [ ! -x "$JS_BINARY__NPM_BINARY" ]; then
        logf_fatal "npm binary '%s' is not executable" "$JS_BINARY__NPM_BINARY"
        exit 1
    fi
fi

if [ "${JS_BINARY__NO_RUNFILES:-}" ]; then
    export JS_BINARY__NODE_WRAPPER
    JS_BINARY__NODE_WRAPPER=$(resolve_execroot_bin_path "{{node_wrapper}}")
else
    export JS_BINARY__NODE_WRAPPER="$JS_BINARY__RUNFILES/{{workspace_name}}/{{node_wrapper}}"
fi
if [ ! -f "$JS_BINARY__NODE_WRAPPER" ]; then
    logf_fatal "node wrapper '%s' not found" "$JS_BINARY__NODE_WRAPPER"
    exit 1
fi
if [ "$_IS_WINDOWS" -ne "1" ] && [ ! -x "$JS_BINARY__NODE_WRAPPER" ]; then
    logf_fatal "node wrapper '%s' is not executable" "$JS_BINARY__NODE_WRAPPER"
    exit 1
fi

if [ "${JS_BINARY__NO_RUNFILES:-}" ]; then
    export JS_BINARY__NODE_PATCHES
    JS_BINARY__NODE_PATCHES=$(resolve_execroot_src_path "{{node_patches}}")
else
    export JS_BINARY__NODE_PATCHES="$JS_BINARY__RUNFILES/{{workspace_name}}/{{node_patches}}"
fi
if [ ! -f "$JS_BINARY__NODE_PATCHES" ]; then
    logf_fatal "node patches '%s' not found" "$JS_BINARY__NODE_PATCHES"
    exit 1
fi

# Change directory to user specified package if set
if [ "${JS_BINARY__CHDIR:-}" ]; then
    logf_debug "changing directory to user specified package %s" "$JS_BINARY__CHDIR"
    cd "$JS_BINARY__CHDIR"
fi

# Gather node options
JS_BINARY__NODE_OPTIONS=()
{{node_options}}

ARGS=()
ALL_ARGS=({{fixed_args}} "$@")
for ARG in ${ALL_ARGS[@]+"${ALL_ARGS[@]}"}; do
    case "$ARG" in
    # Let users pass through arguments to node itself
    --node_options=*) JS_BINARY__NODE_OPTIONS+=("${ARG#--node_options=}") ;;
    # Remaining argv is collected to pass to the program
    *) ARGS+=("$ARG") ;;
    esac
done

# Configure JS_BINARY__FS_PATCH_ROOTS for node fs patches which are run via --require in the node wrapper.
# Don't override JS_BINARY__FS_PATCH_ROOTS if already set by an outer js_binary incase a js_binary such
# as js_run_deverser runs another js_binary tool.
if [ -z "${JS_BINARY__FS_PATCH_ROOTS:-}" ]; then
    JS_BINARY__FS_PATCH_ROOTS="$JS_BINARY__EXECROOT:$JS_BINARY__RUNFILES"
fi
export JS_BINARY__FS_PATCH_ROOTS

# Enable coverage if requested
if [ "${COVERAGE_DIR:-}" ]; then
    logf_debug "enabling v8 coverage support ${COVERAGE_DIR}"
    export NODE_V8_COVERAGE=${COVERAGE_DIR}
fi

# Put the node wrapper directory on the path so that child processes find it first
PATH="$(dirname "$JS_BINARY__NODE_WRAPPER"):$PATH"
export PATH

# Debug logs
if [ "${JS_BINARY__LOG_DEBUG:-}" ]; then
    logf_debug "PATH %s" "$PATH"
    if [ "${BAZEL_BINDIR:-}" ]; then
        logf_debug "BAZEL_BINDIR %s" "$BAZEL_BINDIR"
    fi
    if [ "${BAZEL_BUILD_FILE_PATH:-}" ]; then
        logf_debug "BAZEL_BUILD_FILE_PATH %s" "$BAZEL_BUILD_FILE_PATH"
    fi
    if [ "${BAZEL_COMPILATION_MODE:-}" ]; then
        logf_debug "BAZEL_COMPILATION_MODE %s" "$BAZEL_COMPILATION_MODE"
    fi
    if [ "${BAZEL_INFO_FILE:-}" ]; then
        logf_debug "BAZEL_INFO_FILE %s" "$BAZEL_INFO_FILE"
    fi
    if [ "${BAZEL_PACKAGE:-}" ]; then
        logf_debug "BAZEL_PACKAGE %s" "$BAZEL_PACKAGE"
    fi
    if [ "${BAZEL_TARGET_CPU:-}" ]; then
        logf_debug "BAZEL_TARGET_CPU %s" "$BAZEL_TARGET_CPU"
    fi
    if [ "${BAZEL_TARGET_NAME:-}" ]; then
        logf_debug "BAZEL_TARGET_NAME %s" "$BAZEL_TARGET_NAME"
    fi
    if [ "${BAZEL_VERSION_FILE:-}" ]; then
        logf_debug "BAZEL_VERSION_FILE %s" "$BAZEL_VERSION_FILE"
    fi
    if [ "${BAZEL_WORKSPACE:-}" ]; then
        logf_debug "BAZEL_WORKSPACE %s" "$BAZEL_WORKSPACE"
    fi
    logf_debug "JS_BINARY__FS_PATCH_ROOTS %s" "${JS_BINARY__FS_PATCH_ROOTS:-}"
    logf_debug "JS_BINARY__NODE_PATCHES %s" "${JS_BINARY__NODE_PATCHES:-}"
    logf_debug "JS_BINARY__NODE_OPTIONS %s" "${JS_BINARY__NODE_OPTIONS:-}"
    logf_debug "JS_BINARY__BINDIR %s" "${JS_BINARY__BINDIR:-}"
    logf_debug "JS_BINARY__BUILD_FILE_PATH %s" "${JS_BINARY__BUILD_FILE_PATH:-}"
    logf_debug "JS_BINARY__COMPILATION_MODE %s" "${JS_BINARY__COMPILATION_MODE:-}"
    logf_debug "JS_BINARY__NODE_BINARY %s" "${JS_BINARY__NODE_BINARY:-}"
    logf_debug "JS_BINARY__NODE_WRAPPER %s" "${JS_BINARY__NODE_WRAPPER:-}"
    if [ "${JS_BINARY__NPM_BINARY:-}" ]; then
        logf_debug "JS_BINARY__NPM_BINARY %s" "$JS_BINARY__NPM_BINARY"
    fi
    if [ "${JS_BINARY__NO_RUNFILES:-}" ]; then
        logf_debug "JS_BINARY__NO_RUNFILES %s" "$JS_BINARY__NO_RUNFILES"
    fi
    logf_debug "JS_BINARY__PACKAGE %s" "${JS_BINARY__PACKAGE:-}"
    logf_debug "JS_BINARY__TARGET_CPU %s" "${JS_BINARY__TARGET_CPU:-}"
    logf_debug "JS_BINARY__TARGET_NAME %s" "${JS_BINARY__TARGET_NAME:-}"
    logf_debug "JS_BINARY__WORKSPACE %s" "${JS_BINARY__WORKSPACE:-}"
    logf_debug "js_binary entry point %s" "$entry_point"
    if [ "${JS_BINARY__USE_EXECROOT_ENTRY_POINT:-}" ]; then
        logf_debug "JS_BINARY__USE_EXECROOT_ENTRY_POINT %s" "$JS_BINARY__USE_EXECROOT_ENTRY_POINT"
    fi
fi

# Info logs
if [ "${JS_BINARY__LOG_INFO:-}" ]; then
    if [ "${BAZEL_TARGET:-}" ]; then
        logf_info "BAZEL_TARGET %s" "${BAZEL_TARGET:-}"
    fi
    logf_info "JS_BINARY__TARGET %s" "${JS_BINARY__TARGET:-}"
    logf_info "JS_BINARY__RUNFILES %s" "${JS_BINARY__RUNFILES:-}"
    logf_info "JS_BINARY__EXECROOT %s" "${JS_BINARY__EXECROOT:-}"
    logf_info "PWD %s" "$PWD"
fi

# ==============================================================================
# Run the main program
# ==============================================================================

if [ "${JS_BINARY__LOG_INFO:-}" ]; then
    logf_info "$(echo -n "running" "$JS_BINARY__NODE_WRAPPER" ${JS_BINARY__NODE_OPTIONS[@]+"${JS_BINARY__NODE_OPTIONS[@]}"} -- "$entry_point" ${ARGS[@]+"${ARGS[@]}"})"
fi

set +e

if [ "${STDOUT_CAPTURE:-}" ] && [ "${STDERR_CAPTURE:-}" ]; then
    "$JS_BINARY__NODE_WRAPPER" ${JS_BINARY__NODE_OPTIONS[@]+"${JS_BINARY__NODE_OPTIONS[@]}"} -- "$entry_point" ${ARGS[@]+"${ARGS[@]}"} <&0 >>"$STDOUT_CAPTURE" 2>>"$STDERR_CAPTURE" &
elif [ "${STDOUT_CAPTURE:-}" ]; then
    "$JS_BINARY__NODE_WRAPPER" ${JS_BINARY__NODE_OPTIONS[@]+"${JS_BINARY__NODE_OPTIONS[@]}"} -- "$entry_point" ${ARGS[@]+"${ARGS[@]}"} <&0 >>"$STDOUT_CAPTURE" &
elif [ "${STDERR_CAPTURE:-}" ]; then
    "$JS_BINARY__NODE_WRAPPER" ${JS_BINARY__NODE_OPTIONS[@]+"${JS_BINARY__NODE_OPTIONS[@]}"} -- "$entry_point" ${ARGS[@]+"${ARGS[@]}"} <&0 2>>"$STDERR_CAPTURE" &
else
    "$JS_BINARY__NODE_WRAPPER" ${JS_BINARY__NODE_OPTIONS[@]+"${JS_BINARY__NODE_OPTIONS[@]}"} -- "$entry_point" ${ARGS[@]+"${ARGS[@]}"} <&0 &
fi

# ==============================================================================
# Wait for program to finish
# ==============================================================================

readonly child=$!
# Bash does not forward termination signals to any child process when
# running in docker so need to manually trap and forward the signals
_term() { kill -TERM "${child}" 2>/dev/null; }
_int() { kill -INT "${child}" 2>/dev/null; }
trap _term SIGTERM
trap _int SIGINT
wait "$child"
# Remove trap after first signal has been receieved and wait for child to exit
# (first wait returns immediatel if SIGTERM is received while waiting). Second
# wait is a no-op if child has already terminated.
trap - SIGTERM SIGINT
wait "$child"

RESULT="$?"
set -e

# ==============================================================================
# Mop up after main program
# ==============================================================================

if [ "${JS_BINARY__EXPECTED_EXIT_CODE:-}" ]; then
    if [ "$RESULT" != "$JS_BINARY__EXPECTED_EXIT_CODE" ]; then
        logf_error "expected exit code to be '%s', but got '%s'" "$JS_BINARY__EXPECTED_EXIT_CODE" "$RESULT"
        if [ $RESULT -eq 0 ]; then
            # This exit code is handled specially by Bazel:
            # https://github.com/bazelbuild/bazel/blob/486206012a664ecb20bdb196a681efc9a9825049/src/main/java/com/google/devtools/build/lib/util/ExitCode.java#L44
            readonly BAZEL_EXIT_TESTS_FAILED=3
            exit $BAZEL_EXIT_TESTS_FAILED
        fi
        exit $RESULT
    else
        exit 0
    fi
fi

if [ "${JS_BINARY__EXIT_CODE_OUTPUT_FILE:-}" ]; then
    # Exit zero if the exit code was captured
    echo -n "$RESULT" >"$JS_BINARY__EXIT_CODE_OUTPUT_FILE"
    exit 0
else
    exit $RESULT
fi
