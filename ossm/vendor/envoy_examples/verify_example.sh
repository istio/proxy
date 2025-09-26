#!/bin/bash -e

EXAMPLE_NAME="${1}"
EXAMPLE_DIR="${2}"
TMPOUT=$(mktemp)
RUNDIR=$(mktemp -d)

export COMPOSE_INTERACTIVE_NO_CLI=1
export COMPOSE_PROGRESS=quiet

complete () {
    EXITCODE="$(tail -n 1 "${TMPOUT}" | grep -oP '(?<=COMMAND_EXIT_CODE=")[0-9]+')"
    echo "${EXAMPLE_NAME}: ${EXITCODE}:${TIME}"
    tail -n 1 "$TMPOUT"
    echo
    echo
    if [[ "$EXITCODE" != 0 ]]; then
        cat "$TMPOUT"
    fi
    echo
    echo
    rm -rf "$RUNDIR"
    rm -rf "$TMPOUT"
}

verify () {
    tar xf "$EXAMPLE_DIR" -C "$RUNDIR"
    export DOCKER_NO_PULL=1
    export DOCKER_RMI_CLEANUP=1
    # This is set to simulate an environment where users have shared home drives protected
    # by a strong umask (ie only group readable by default).
    umask 027
    chmod -R o-rwx "$RUNDIR"
    cd "$RUNDIR"
    dirlist=$(ls .)
    if [[ "$dirlist" == "external" ]]; then
        cd "external/envoy_examples/${EXAMPLE_NAME}"
    else
        cd "${EXAMPLE_NAME}"
    fi
    script -q -c "./verify.sh" "$TMPOUT" >/dev/null
}

trap complete EXIT

TIME=$({ time verify; } 2>&1 | grep real | cut -dl -f2)
