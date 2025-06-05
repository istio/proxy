#!/bin/sh
set -e

STDIN=$(cat)

cat <<EOF
from cffi import FFI
ffi = FFI()
ffi.cdef("""
${STDIN}
""")
C = None
for path in [ # Search for libcircllhist.so
    "./libcircllhist.so", # 1. cwd
    "/opt/circonus/lib/libcircllhist.so", # 2. vendor path
    "libcircllhist.so" # 3. system paths via ld.so
    ]:
    try:
        C = ffi.dlopen(path)
        break
    except OSError:
        pass

if not C:
    # let dlopen throw it's error
    print("""

libcircllhist.so was not found on your system.
Please install libcircllhist from: https://github.com/circonus-labs/libcircllhist/

    """)
    ffi.dlopen("libcircllhist.so")
EOF
