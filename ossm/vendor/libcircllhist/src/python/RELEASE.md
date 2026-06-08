# Release procedure

Releases are handled via [Github Workflow](../../.github/workflows/publish-to-pypi.yml)

* Edit setup.py and increase version number
* Commit the change with a message: `Version bump to <version>`
* The workflow will build and upload to Test PyPI
* Assuming workflow succeeds, tag as `py-<version>` and push.
* Workflow will see the tag and upload to the official PyPI

## Manual Process

Do not use this unless there is some sort of outage with GH Workflows or some
other emergency.

* Do a clean build of the C library to ensure the FFI wrapper is up to date
* Perform the package build: `python -m build`
  * To test: `twine upload --repository testpypi dist/*`
  * For real: `twine upload dist/*`
