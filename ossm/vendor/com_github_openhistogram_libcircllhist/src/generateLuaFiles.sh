#!/bin/sh
set -e

STDIN=$(cat)

cat <<EOF
local ffi = require('ffi');

--------------------------------------------------------------------------------
-- circllhist.h
--------------------------------------------------------------------------------
ffi.cdef[[
${STDIN}
]]

return ffi.load("libcircllhist")
EOF
