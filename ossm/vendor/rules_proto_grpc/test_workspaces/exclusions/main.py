# Attempt to import proto file, this should succeed
import demo_pb2

# Scan generated files, we should not have generated a 'google' directory if
# exclusions are in effect
import pathlib
import sys
if pathlib.Path('py_lib_pb/google').exists():
    print('Excluded files have been generated')
    sys.exit(1)
