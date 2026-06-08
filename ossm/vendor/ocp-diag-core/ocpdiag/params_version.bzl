# Copyright 2022 Google LLC
#
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.

"""Rule for generating a header file containing the version of the OCPDiag params library"""

def is_bazel():
    """Determines whether this build is run using bazel or internal tools."""
    return not hasattr(native, "genmpm")

def get_change_number_command():
    """Generates a command that gets the change number from workspace.

    Returns:
        A bash script that gets the change number as variable `change_number`.
    """
    if is_bazel():
        key = "BUILD_PARAMS_VERSION"
        workspace_file_path = "bazel-out/volatile-status.txt"
    else:
        key = "BUILD_CHANGELIST"
        workspace_file_path = "blaze-out/build-changelist.txt"

    # Note that awk command returns the value from the key-value pair
    # containing the version
    return """
    key_val_pair=`cat {} | grep "{}" || true`
    if [ -n "$$key_val_pair" ]
    then
        change_number=`echo "$$key_val_pair" | awk '{{printf $$2}}'`
    else
        change_number="UNKNOWN"
    fi
    """.format(workspace_file_path, key)

def get_params_version_command():
    """Generates a command that creates the OCPDiag params version file.

    This is done by using get_change_number_command() to extract the variable
    $change_number and then writing it as a constant to a C++ header file.

    Returns:
        A bash script that creates the header file as a string to be included
        in a genrule.
    """
    change_number_cmd = get_change_number_command()
    build_header_cmd = """
    echo "constexpr char kVersionString[] = \\"$$change_number\\";" >> $@
    """

    return change_number_cmd + build_header_cmd
