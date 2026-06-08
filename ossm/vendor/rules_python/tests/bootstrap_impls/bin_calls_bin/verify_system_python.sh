#!/bin/bash
set -euo pipefail

source "$(dirname "$0")/verify.sh"
verify_output "$(dirname "$0")/outer_calls_inner_system_python.out"
