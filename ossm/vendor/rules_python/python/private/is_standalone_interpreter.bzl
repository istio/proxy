# Copyright 2024 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""This file contains repository rules and macros to support toolchain registration.
"""

load(":repo_utils.bzl", "repo_utils")

STANDALONE_INTERPRETER_FILENAME = "STANDALONE_INTERPRETER"

def is_standalone_interpreter(rctx, python_interpreter_path, *, logger = None):
    """Query a python interpreter target for whether or not it's a rules_rust provided toolchain

    Args:
        rctx: {type}`repository_ctx` The repository rule's context object.
        python_interpreter_path: {type}`path` A path representing the interpreter.
        logger: Optional logger to use for operations.

    Returns:
        {type}`bool` Whether or not the target is from a rules_python generated toolchain.
    """

    # Only update the location when using a hermetic toolchain.
    if not python_interpreter_path:
        return False

    # This is a rules_python provided toolchain.
    return repo_utils.execute_unchecked(
        rctx,
        op = "IsStandaloneInterpreter",
        arguments = [
            "ls",
            "{}/{}".format(
                python_interpreter_path.dirname,
                STANDALONE_INTERPRETER_FILENAME,
            ),
        ],
        logger = logger,
    ).return_code == 0
