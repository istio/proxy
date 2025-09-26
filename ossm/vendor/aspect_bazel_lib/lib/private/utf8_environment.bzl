# Vendored from
# https://github.com/bazelbuild/rules_java/blob/c22454fadb4773cbd202bfa3e28f1d6a88c4c94a/toolchains/utf8_environment.bzl
#
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

"""
Determines the environment required for actions to support UTF-8.
"""

Utf8EnvironmentInfo = provider(
    doc = "The environment required for actions to support UTF-8.",
    fields = {
        "environment": "The environment to use for actions to support UTF-8.",
    },
)

# NB: https://github.com/bazelbuild/rules_java/blob/c22454fadb4773cbd202bfa3e28f1d6a88c4c94a/toolchains/utf8_environment.bzl
# uses LC_CTYPE which is technically more correct:
# > The LC_CTYPE category determines character handling rules governing the interpretation of
# > sequences of bytes of text data characters (that is, single-byte versus multibyte characters),
# > the classification of characters (for example, alpha, digit, and so on),
# > and the behavior of character classes.
#
# However libarchive only looks for LC_ALL, otherwise it spams warnings like "tar: Failed to set default locale"
# https://github.com/libarchive/libarchive/blob/65196fdd1a385f22114f245a9002ee8dc899f2c4/tar/bsdtar.c#L192
# Docs for LC_ALL:
# > Overrides the value of the LANG environment variable and the values of any other LC_* environment variables.
_LOCALE_VAR = "LC_ALL"

# The default UTF-8 locale on all recent Linux distributions. It is also available in Cygwin and
# MSYS2, but doesn't matter for determining the bsdtar encoding on Windows, which always
# uses the active code page.
_DEFAULT = "C.UTF-8"

# macOS doesn't have the C.UTF-8 locale, but en_US.UTF-8 is available and works the same way.
_MACOS = "en_US.UTF-8"

def _utf8_environment_impl(ctx):
    is_macos = ctx.target_platform_has_constraint(ctx.attr._macos_constraint[platform_common.ConstraintValueInfo])
    return Utf8EnvironmentInfo(environment = {_LOCALE_VAR: _MACOS if is_macos else _DEFAULT})

utf8_environment = rule(
    _utf8_environment_impl,
    attrs = {
        "_macos_constraint": attr.label(default = "@platforms//os:macos"),
    },
    doc = "Returns a suitable environment for actions to support UTF-8.",
)
