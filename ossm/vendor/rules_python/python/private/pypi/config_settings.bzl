# Copyright 2024 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
The {obj}`config_settings` macro is used to create the config setting targets
that can be used in the {obj}`pkg_aliases` macro for selecting the compatible
repositories.

Bazel's selects work by selecting the most-specialized configuration setting
that matches the target platform, which is further described in [bazel documentation][docs].
We can leverage this fact to ensure that the most specialized matches are used
by default with the users being able to configure string_flag values to select
the less specialized ones.

[docs]: https://bazel.build/docs/configurable-attributes

The config settings in the order from the least specialized to the most
specialized is as follows:
* `:is_cp3<minor_version><suffix>`
* `:is_cp3<minor_version>_sdist<suffix>`
* `:is_cp3<minor_version>_py_none_any<suffix>`
* `:is_cp3<minor_version>_py3_none_any<suffix>`
* `:is_cp3<minor_version>_py3_abi3_any<suffix>`
* `:is_cp3<minor_version>_none_any<suffix>`
* `:is_cp3<minor_version>_any_any<suffix>`
* `:is_cp3<minor_version>_cp3<minor_version>_any<suffix>` and `:is_cp3<minor_version>_cp3<minor_version>t_any<suffix>`
* `:is_cp3<minor_version>_py_none_<platform_suffix>`
* `:is_cp3<minor_version>_py3_none_<platform_suffix>`
* `:is_cp3<minor_version>_py3_abi3_<platform_suffix>`
* `:is_cp3<minor_version>_none_<platform_suffix>`
* `:is_cp3<minor_version>_abi3_<platform_suffix>`
* `:is_cp3<minor_version>_cp3<minor_version>_<platform_suffix>` and `:is_cp3<minor_version>_cp3<minor_version>t_<platform_suffix>`

The specialization of free-threaded vs non-free-threaded wheels is the same as
they are just variants of each other. The same goes for the specialization of
`musllinux` vs `manylinux`.

The goal of this macro is to provide config settings that provide unambigous
matches if any pair of them is used together for any target configuration
setting. We achieve this by using dummy internal `flag_values` keys to force the
items further down the list to appear to be more specialized than the ones above.

What is more, the names of the config settings are as similar to the platform wheel
specification as possible. How the wheel names map to the config setting names defined
in here is described in {obj}`pkg_aliases` documentation.

:::{note}
Right now the specialization of adjacent config settings where one is with
`constraint_values` and one is without is ambiguous. I.e. `py_none_any` and
`sdist_linux_x86_64` have the same specialization from bazel point of view
because one has one `flag_value` entry and `constraint_values` and the
other has 2 flag_value entries. And unfortunately there is no way to disambiguate
it, because we are essentially in two dimensions here (`flag_values` and
`constraint_values`). Hence, when using the `config_settings` from here,
either have all of them with empty `suffix` or all of them with a non-empty
suffix.
:::
"""

load("//python/private:flags.bzl", "LibcFlag")
load(":flags.bzl", "INTERNAL_FLAGS", "UniversalWhlFlag")

FLAGS = struct(
    **{
        f: str(Label("//python/config_settings:" + f))
        for f in [
            "is_pip_whl_auto",
            "is_pip_whl_no",
            "is_pip_whl_only",
            "is_py_freethreaded",
            "is_py_non_freethreaded",
            "pip_whl_glibc_version",
            "pip_whl_muslc_version",
            "pip_whl_osx_arch",
            "pip_whl_osx_version",
            "py_linux_libc",
            "python_version",
        ]
    }
)

_DEFAULT = "//conditions:default"
_INCOMPATIBLE = "@platforms//:incompatible"

# Here we create extra string flags that are just to work with the select
# selecting the most specialized match. We don't allow the user to change
# them.
_flags = struct(
    **{
        f: str(Label("//python/config_settings:_internal_pip_" + f))
        for f in INTERNAL_FLAGS
    }
)

def config_settings(
        *,
        python_versions = [],
        glibc_versions = [],
        muslc_versions = [],
        osx_versions = [],
        target_platforms = [],
        name = None,
        **kwargs):
    """Generate all of the pip config settings.

    Args:
        name (str): Currently unused.
        python_versions (list[str]): The list of python versions to configure
            config settings for.
        glibc_versions (list[str]): The list of glibc version of the wheels to
            configure config settings for.
        muslc_versions (list[str]): The list of musl version of the wheels to
            configure config settings for.
        osx_versions (list[str]): The list of OSX OS versions to configure
            config settings for.
        target_platforms (list[str]): The list of "{os}_{cpu}" for deriving
            constraint values for each condition.
        **kwargs: Other args passed to the underlying implementations, such as
            {obj}`native`.
    """

    glibc_versions = [""] + glibc_versions
    muslc_versions = [""] + muslc_versions
    osx_versions = [""] + osx_versions
    target_platforms = [("", ""), ("osx", "universal2")] + [
        t.split("_", 1)
        for t in target_platforms
    ]

    for python_version in python_versions:
        for os, cpu in target_platforms:
            constraint_values = []
            suffix = ""
            if os:
                constraint_values.append("@platforms//os:" + os)
                suffix += "_" + os
            if cpu:
                suffix += "_" + cpu
                if cpu != "universal2":
                    constraint_values.append("@platforms//cpu:" + cpu)

            _dist_config_settings(
                suffix = suffix,
                plat_flag_values = _plat_flag_values(
                    os = os,
                    cpu = cpu,
                    osx_versions = osx_versions,
                    glibc_versions = glibc_versions,
                    muslc_versions = muslc_versions,
                ),
                constraint_values = constraint_values,
                python_version = python_version,
                **kwargs
            )

def _dist_config_settings(*, suffix, plat_flag_values, python_version, **kwargs):
    flag_values = {
        Label("//python/config_settings:python_version_major_minor"): python_version,
    }

    cpv = "cp" + python_version.replace(".", "")
    prefix = "is_{}".format(cpv)

    _dist_config_setting(
        name = prefix + suffix,
        flag_values = flag_values,
        **kwargs
    )

    flag_values[_flags.dist] = ""

    # First create an sdist, we will be building upon the flag values, which
    # will ensure that each sdist config setting is the least specialized of
    # all. However, we need at least one flag value to cover the case where we
    # have `sdist` for any platform, hence we have a non-empty `flag_values`
    # here.
    _dist_config_setting(
        name = "{}_sdist{}".format(prefix, suffix),
        flag_values = flag_values,
        compatible_with = (FLAGS.is_pip_whl_no, FLAGS.is_pip_whl_auto),
        **kwargs
    )

    used_flags = {}

    # NOTE @aignas 2024-12-01: the abi3 is not compatible with freethreaded
    # builds as per PEP703 (https://peps.python.org/pep-0703/#backwards-compatibility)
    #
    # The discussion here also reinforces this notion:
    # https://discuss.python.org/t/pep-703-making-the-global-interpreter-lock-optional-3-12-updates/26503/99

    for name, f, compatible_with in [
        ("py_none", _flags.whl, None),
        ("py3_none", _flags.whl_py3, None),
        ("py3_abi3", _flags.whl_py3_abi3, (FLAGS.is_py_non_freethreaded,)),
        ("none", _flags.whl_pycp3x, None),
        ("abi3", _flags.whl_pycp3x_abi3, (FLAGS.is_py_non_freethreaded,)),
        # The below are not specializations of one another, they are variants
        (cpv, _flags.whl_pycp3x_abicp, (FLAGS.is_py_non_freethreaded,)),
        (cpv + "t", _flags.whl_pycp3x_abicp, (FLAGS.is_py_freethreaded,)),
    ]:
        if (f, compatible_with) in used_flags:
            # This should never happen as all of the different whls should have
            # unique flag values
            fail("BUG: the flag {} is attempted to be added twice to the list".format(f))
        else:
            flag_values[f] = "yes" if f == _flags.whl else ""
            used_flags[(f, compatible_with)] = True

        _dist_config_setting(
            name = "{}_{}_any{}".format(prefix, name, suffix),
            flag_values = flag_values,
            compatible_with = compatible_with,
            **kwargs
        )

    generic_flag_values = flag_values
    generic_used_flags = used_flags

    for (suffix, flag_values) in plat_flag_values:
        used_flags = {(f, None): True for f in flag_values} | generic_used_flags
        flag_values = flag_values | generic_flag_values

        for name, f, compatible_with in [
            ("py_none", _flags.whl_plat, None),
            ("py3_none", _flags.whl_plat_py3, None),
            ("py3_abi3", _flags.whl_plat_py3_abi3, (FLAGS.is_py_non_freethreaded,)),
            ("none", _flags.whl_plat_pycp3x, None),
            ("abi3", _flags.whl_plat_pycp3x_abi3, (FLAGS.is_py_non_freethreaded,)),
            # The below are not specializations of one another, they are variants
            (cpv, _flags.whl_plat_pycp3x_abicp, (FLAGS.is_py_non_freethreaded,)),
            (cpv + "t", _flags.whl_plat_pycp3x_abicp, (FLAGS.is_py_freethreaded,)),
        ]:
            if (f, compatible_with) in used_flags:
                # This should never happen as all of the different whls should have
                # unique flag values.
                fail("BUG: the flag {} is attempted to be added twice to the list".format(f))
            else:
                flag_values[f] = ""
                used_flags[(f, compatible_with)] = True

            _dist_config_setting(
                name = "{}_{}_{}".format(prefix, name, suffix),
                flag_values = flag_values,
                compatible_with = compatible_with,
                **kwargs
            )

def _to_version_string(version, sep = "."):
    if not version:
        return ""

    return "{}{}{}".format(version[0], sep, version[1])

def _plat_flag_values(os, cpu, osx_versions, glibc_versions, muslc_versions):
    ret = []
    if os == "":
        return []
    elif os == "windows":
        ret.append(("{}_{}".format(os, cpu), {}))
    elif os == "osx":
        for osx_version in osx_versions:
            flags = {
                FLAGS.pip_whl_osx_version: _to_version_string(osx_version),
            }
            if cpu != "universal2":
                flags[FLAGS.pip_whl_osx_arch] = UniversalWhlFlag.ARCH

            if not osx_version:
                suffix = "{}_{}".format(os, cpu)
            else:
                suffix = "{}_{}_{}".format(os, _to_version_string(osx_version, "_"), cpu)

            ret.append((suffix, flags))

    elif os == "linux":
        for os_prefix, linux_libc in {
            os: LibcFlag.GLIBC,
            "many" + os: LibcFlag.GLIBC,
            "musl" + os: LibcFlag.MUSL,
        }.items():
            if linux_libc == LibcFlag.GLIBC:
                libc_versions = glibc_versions
                libc_flag = FLAGS.pip_whl_glibc_version
            elif linux_libc == LibcFlag.MUSL:
                libc_versions = muslc_versions
                libc_flag = FLAGS.pip_whl_muslc_version
            else:
                fail("Unsupported libc type: {}".format(linux_libc))

            for libc_version in libc_versions:
                if libc_version and os_prefix == os:
                    continue
                elif libc_version:
                    suffix = "{}_{}_{}".format(os_prefix, _to_version_string(libc_version, "_"), cpu)
                else:
                    suffix = "{}_{}".format(os_prefix, cpu)

                ret.append((
                    suffix,
                    {
                        FLAGS.py_linux_libc: linux_libc,
                        libc_flag: _to_version_string(libc_version),
                    },
                ))
    else:
        fail("Unsupported os: {}".format(os))

    return ret

def _dist_config_setting(*, name, compatible_with = None, native = native, **kwargs):
    """A macro to create a target for matching Python binary and source distributions.

    Args:
        name: The name of the public target.
        compatible_with: {type}`tuple[Label]` A collection of config settings that are
            compatible with the given dist config setting. For example, if only
            non-freethreaded python builds are allowed, add
            FLAGS.is_py_non_freethreaded here.
        native (struct): The struct containing alias and config_setting rules
            to use for creating the objects. Can be overridden for unit tests
            reasons.
        **kwargs: The kwargs passed to the config_setting rule. Visibility of
            the main alias target is also taken from the kwargs.
    """
    if compatible_with:
        dist_config_setting_name = "_" + name
        native.alias(
            name = name,
            actual = select(
                {setting: dist_config_setting_name for setting in compatible_with} | {
                    _DEFAULT: _INCOMPATIBLE,
                },
            ),
            visibility = kwargs.get("visibility"),
        )
        name = dist_config_setting_name

    native.config_setting(name = name, **kwargs)
