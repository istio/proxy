# Copyright 2017 The Bazel Authors. All rights reserved.
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

"""Genrule which provides Apple's Xcode environment."""

load(
    "//rules/private:apple_genrule.bzl",
    _apple_genrule_inner = "apple_genrule",
)

# buildozer: disable=function-docstring-args
def apple_genrule(name, **kwargs):
    """Genrule which provides Apple specific environment and make variables."""

    # This split/indirection traces back to cl/128714692 and b/30413353.
    if kwargs.get("executable", False):
        outs = kwargs.pop("outs", [])
        if len(outs) != 1:
            fail("apple_genrule, if executable, must have exactly one output")

        intermediate_out = outs[0] + ".nonexecutable"

        # Remove any visibility and make this sub rule private since it is an
        # implementation detail.
        sub_kwargs = dict(kwargs)
        sub_kwargs.pop("visibility", None)
        _apple_genrule_inner(
            name = name + "_nonexecutable",
            outs = [intermediate_out],
            visibility = ["//visibility:private"],
            **sub_kwargs
        )

        # Remove anything from kwargs that might have a meaning that isn't wanted
        # on the genrule that does the copy. Generally, we are just trying to
        # keep things like testonly, visibility, etc.
        kwargs.pop("stamp", None)
        kwargs.pop("cmd", None)
        kwargs.pop("executable", None)
        kwargs.pop("srcs", None)
        kwargs.pop("message", None)
        kwargs.pop("tools", None)
        kwargs.pop("no_sandbox", None)
        native.genrule(
            name = name,
            outs = outs,
            srcs = [intermediate_out],
            cmd = "cp $< $@",
            executable = True,
            **kwargs
        )
    else:
        _apple_genrule_inner(name = name, **kwargs)
