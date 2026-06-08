import runpy
import shutil
import sys

try:
    sys.argv.pop(0)  # Remove zipapp_stage2_bootstrap from args
    runpy.run_path(sys.argv[0], run_name="__main__")
finally:
    if zip_dir := sys._xoptions.get("RULES_PYTHON_ZIP_DIR"):
        shutil.rmtree(zip_dir, True)
