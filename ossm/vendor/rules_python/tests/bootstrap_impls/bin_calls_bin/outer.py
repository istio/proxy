import os
import subprocess
import sys

if __name__ == "__main__":
    module_space = os.environ.get("RULES_PYTHON_TESTING_MODULE_SPACE")
    print(f"outer: RULES_PYTHON_TESTING_MODULE_SPACE='{module_space}'")

    inner_binary_path = sys.argv[1]
    result = subprocess.run(
        [inner_binary_path],
        capture_output=True,
        text=True,
        check=True,
    )
    print(result.stdout, end="")
    if result.stderr:
        print(result.stderr, end="", file=sys.stderr)
