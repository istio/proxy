# Don't ignore setup.py files

Make sure that files named `setup.py` are processed by Gazelle.

It's believed that `setup.py` was originally ignored because it, when found
in the repository root directory, is part of the `setuptools` build system
and could cause some issues for Gazelle. However, files within source code can
also be called `setup.py` and thus should be processed by Gazelle.
