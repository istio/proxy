import os

module_space = os.environ.get("RULES_PYTHON_TESTING_MODULE_SPACE")
print(f"inner: RULES_PYTHON_TESTING_MODULE_SPACE='{module_space}'")
