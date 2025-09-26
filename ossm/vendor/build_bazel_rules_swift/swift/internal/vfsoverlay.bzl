# Copyright 2020 The Bazel Authors. All rights reserved.
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

"""Logic for generating VFS overlay files."""

def write_vfsoverlay(
        actions,
        swiftmodules,
        vfsoverlay_file,
        virtual_swiftmodule_root):
    """Generates a VFS overlay and writes it to a file.

    Args:
        actions: The object used to register actions.
        swiftmodules: The `list` of `.swiftmodule` files to include in the
             VFS overlay.
        vfsoverlay_file: A `File` representing the VFS overlay to be written.
        virtual_swiftmodule_root: The rooted path fragment representing the
             directory in the VFS where all `.swiftmodule` files will be placed.
    """
    virtual_swiftmodules = [
        {
            "type": "file",
            "name": swiftmodule.basename,
            "external-contents": swiftmodule.path,
        }
        for swiftmodule in swiftmodules
    ]

    # These explicit settings ensure that the VFS actually improves search
    # performance.
    vfsoverlay_object = {
        "version": 0,
        "case-sensitive": True,
        "overlay-relative": False,
        "use-external-names": False,
        "roots": [
            {
                "type": "directory",
                "name": virtual_swiftmodule_root,
                "contents": virtual_swiftmodules,
            },
        ],
    }

    # The YAML specification defines it has a superset of JSON, so it is safe to
    # use the built-in `to_json` function here.
    vfsoverlay_yaml = json.encode(struct(**vfsoverlay_object))

    actions.write(
        content = vfsoverlay_yaml,
        output = vfsoverlay_file,
    )
