# Test that for a py_binary from a dependency module, we place links created via
# runfiles(...) in the right place. This tests the fix made for issues/3503

set -eu
echo "[*] Testing running the binary"
"$VENV_BIN"
