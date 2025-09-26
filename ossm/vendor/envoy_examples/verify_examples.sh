#!/bin/bash -e

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'
RETURNS=0

trim () {
    text=$(< /dev/stdin)
    echo -n "$text" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//'
}

pass () {
    local timing="${1}"
    echo "${GREEN}passed${NC} in ${timing}"
}

fail () {
    local timing="${1}" exitcode="${2}"
    echo "${RED}failed (${exitcode})${NC} in ${timing}"
}

for result in "$@"; do
    RESULT="$(head -n1 "$result")"
    NAME="$(echo "${RESULT}" | cut -d: -f1 | trim)"
    EXITCODE="$(echo "${RESULT}" | cut -d: -f2 | trim)"
    if [[ $EXITCODE != 0 ]]; then
        echo
        echo -e "============= ${RED}FAILED (${NAME})${NC} ===================="
        tail -n+3 "$result"
        echo -e "============ ${RED}/FAILED (${NAME})${NC} ===================="
        echo
    fi
done

for result in "$@"; do
    RESULT=$(head -n1 "$result")
    NAME="$(echo "${RESULT}" | cut -d: -f1 | trim)"
    EXITCODE="$(echo "${RESULT}" | cut -d: -f2 | trim)"
    TIME="$(echo "${RESULT}" | cut -d: -f3 | trim)"
    OUTCOME=$(pass "${TIME}")
    if [[ $EXITCODE != 0 ]]; then
        OUTCOME=$(fail "${TIME}" "$EXITCODE")
        RETURNS=1
    fi
    echo -e "${NAME}: ${OUTCOME}"
done

exit "${RETURNS}"
