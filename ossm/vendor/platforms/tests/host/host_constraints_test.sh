echo actual host constraints: ${ACTUAL_HOST_CONSTRAINTS}
echo expected host constraints: ${EXPECTED_HOST_CONSTRAINTS}
test "${ACTUAL_HOST_CONSTRAINTS}" == "${EXPECTED_HOST_CONSTRAINTS}"
