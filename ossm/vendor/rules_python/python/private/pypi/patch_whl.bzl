# Copyright 2023 The Bazel Authors. All rights reserved.
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

"""A small utility to patch a file in the repository context and repackage it using a Python interpreter

Note, because we are patching a wheel file and we need a new RECORD file, this
function will print a diff of the RECORD and will ask the user to include a
RECORD patch in their patches that they maintain. This is to ensure that we can
satisfy the following usecases:
* Patch an invalid RECORD file.
* Patch files within a wheel.

If we were silently regenerating the RECORD file, we may be vulnerable to supply chain
attacks (it is a very small chance) and keeping the RECORD patches next to the
other patches ensures that the users have overview on exactly what has changed
within the wheel.
"""

load(":parse_whl_name.bzl", "parse_whl_name")
load(":pypi_repo_utils.bzl", "pypi_repo_utils")

_rules_python_root = Label("//:BUILD.bazel")

def patched_whl_name(original_whl_name):
    """Return the new filename to output the patched wheel.

    Args:
        original_whl_name: {type}`str` the whl name of the original file.

    Returns:
        {type}`str` an output name to write the patched wheel to.
    """
    parsed_whl = parse_whl_name(original_whl_name)
    version = parsed_whl.version
    suffix = "patched"
    if "+" in version:
        # This already has some local version, so we just append one more
        # identifier here. We comply with the spec and mark the file as patched
        # by adding a local version identifier at the end.
        #
        # By doing this we can still install the package using most of the package
        # managers
        #
        # See https://packaging.python.org/en/latest/specifications/version-specifiers/#local-version-identifiers
        version = "{}.{}".format(version, suffix)
    else:
        version = "{}+{}".format(version, suffix)

    return "{distribution}-{version}-{python_tag}-{abi_tag}-{platform_tag}.whl".format(
        distribution = parsed_whl.distribution,
        version = version,
        python_tag = parsed_whl.python_tag,
        abi_tag = parsed_whl.abi_tag,
        platform_tag = parsed_whl.platform_tag,
    )

def patch_whl(rctx, *, python_interpreter, whl_path, patches, **kwargs):
    """Patch a whl file and repack it to ensure that the RECORD metadata stays correct.

    Args:
        rctx: repository_ctx
        python_interpreter: the python interpreter to use.
        whl_path: The whl file name to be patched.
        patches: a label-keyed-int dict that has the patch files as keys and
            the patch_strip as the value.
        **kwargs: extras passed to repo_utils.execute_checked.

    Returns:
        value of the repackaging action.
    """

    # extract files into the current directory for patching as rctx.patch
    # does not support patching in another directory.
    whl_input = rctx.path(whl_path)

    # symlink to a zip file to use bazel's extract so that we can use bazel's
    # repository_ctx patch implementation. The whl file may be in a different
    # external repository.
    whl_file_zip = whl_input.basename + ".zip"
    rctx.symlink(whl_input, whl_file_zip)
    rctx.extract(whl_file_zip)
    if not rctx.delete(whl_file_zip):
        fail("Failed to remove the symlink after extracting")

    if not patches:
        fail("Trying to patch wheel without any patches")

    for patch_file, patch_strip in patches.items():
        rctx.patch(patch_file, strip = patch_strip)

    record_patch = rctx.path("RECORD.patch")
    whl_patched = patched_whl_name(whl_input.basename)

    pypi_repo_utils.execute_checked(
        rctx,
        python = python_interpreter,
        srcs = [
            Label("//python/private/pypi:repack_whl.py"),
            Label("//tools:wheelmaker.py"),
        ],
        arguments = [
            "-m",
            "python.private.pypi.repack_whl",
            "--record-patch",
            record_patch,
            whl_input,
            whl_patched,
        ],
        environment = {
            "PYTHONPATH": str(rctx.path(_rules_python_root).dirname),
        },
        **kwargs
    )

    if record_patch.exists:
        record_patch_contents = rctx.read(record_patch)
        warning_msg = """WARNING: the resultant RECORD file of the patch wheel is different

    If you are patching on Windows, you may see this warning because of
    a known issue (bazel-contrib/rules_python#1639) with file endings.

    If you would like to silence the warning, you can apply the patch that is stored in
      {record_patch}. The contents of the file are below:
{record_patch_contents}""".format(
            record_patch = record_patch,
            record_patch_contents = record_patch_contents,
        )
        print(warning_msg)  # buildifier: disable=print

    return rctx.path(whl_patched)
