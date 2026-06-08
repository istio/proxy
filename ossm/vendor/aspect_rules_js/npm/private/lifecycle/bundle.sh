#!/usr/bin/env bash
set -o errexit -o nounset
# Bundle our program with its two dependencies, because they're large:
# ┌─────────────────┬──────────────┬────────┐
# │ name            │ children     │ size   │
# ├─────────────────┼──────────────┼────────┤
# │ @pnpm/lifecycle │ 277          │ 14.71M │
# ├─────────────────┼──────────────┼────────┤
# │ @pnpm/logger    │ 15           │ 0.56M  │
# ├─────────────────┼──────────────┼────────┤
# │ 2 modules       │ 202 children │ 11.73M │
# └─────────────────┴──────────────┴────────┘
# This avoids users having to fetch all those packages just to run the postinstall hooks.

npm install
npx -y rollup -c
# ascii_only avoids bad unicode conversions, fixing
# https://github.com/aspect-build/rules_js/issues/45
npx -y terser@5.12.1 min/index.js -b ascii_only=true >min/index.min.js
rm min/index.js
