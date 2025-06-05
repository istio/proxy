# Standard cpu name constraint_setting and constraint_values
licenses(["notice"])

package(
    default_visibility = ["//visibility:public"],
)

filegroup(
    name = "srcs",
    srcs = glob(["**"]),
)

# To add a new constraint_value see https://github.com/bazelbuild/platforms.
constraint_setting(name = "cpu")

# New cpu values should refer to specific, highly available CPU implementations,
# not broad architectures.  It should be possible to select the right compiler
# options by just by knowing the cpu. This can be a difficult evaluation for
# ARM variations, where there are many possibilities for customization within
# an architecture.
#
# 1. No private names are be allowed. If you build your own custom ARM chips,
#    then define them locally within your organization.
# 2. All CPU values must be clear that they are for a 32 or a 64 bit
#    implementation. For example, cortex-r52 is a 32 bit processor, and
#    cortex-r82 is a 64 bit processor, but both are armv8-r architecture.
#    We use the specific processor names because armv8-r is insufficient to
#    select proper compiler options.
#
# Many of the name here are legacy values and probably violate these conditions.
# We'll try to clean those up over time.

# Special case: Architecture-independent outputs only
#
# Some builds are expected to only produce architecture-independent data files,
# such as configuration files, database seed data, composited images, or even
# some kinds of interpreted scripts (Shell, Python, Perl, etc).
#
# When such a build is being performed, this constraint value may be used to
# ensure that architecture-dependent builds cannot be performed (except by way
# of another transition).
#
# As a final example, consider building a package of NIC firmware images for
# many different NIC models.  The package overall is architecture-independent,
# and should be built with `//cpu:all`, then each individual image rule has a
# transition to the suitable architecture for that specific NIC model.
constraint_value(
    name = "all",
    constraint_setting = ":cpu",
)

constraint_value(
    name = "aarch32",
    constraint_setting = ":cpu",
)

constraint_value(
    name = "aarch64",
    constraint_setting = ":cpu",
)

alias(
    name = "arm",
    actual = ":aarch32",
)

# Cortex-M0, Cortex-M0+, Cortex-M1
constraint_value(
    name = "armv6-m",  # Commonly known as thumbv6
    constraint_setting = ":cpu",
)

# Cortex-M3
constraint_value(
    name = "armv7-m",
    constraint_setting = ":cpu",
)

# Cortex-M4, Cortex-M7
constraint_value(
    name = "armv7e-m",
    constraint_setting = ":cpu",
)

# Cortex-M4, Cortex-M7 with fpu
constraint_value(
    name = "armv7e-mf",  # armv7e-m with fpu
    constraint_setting = ":cpu",
)

# Cortex-M23, Cortex-M33, Cortex-M35P
constraint_value(
    name = "armv8-m",
    constraint_setting = ":cpu",
)

alias(
    name = "arm64",
    actual = ":aarch64",
)

constraint_value(
    name = "arm64_32",
    constraint_setting = ":cpu",
)

constraint_value(
    name = "arm64e",
    constraint_setting = ":cpu",
)

constraint_value(
    name = "armv7",
    constraint_setting = ":cpu",
)

constraint_value(
    name = "armv7k",
    constraint_setting = ":cpu",
)

constraint_value(
    name = "cortex-r52",
    constraint_setting = ":cpu",
)

constraint_value(
    name = "cortex-r82",
    constraint_setting = ":cpu",
)

constraint_value(
    name = "i386",
    constraint_setting = ":cpu",
)

constraint_value(
    name = "ppc",
    constraint_setting = ":cpu",
)

constraint_value(
    name = "ppc32",
    constraint_setting = ":cpu",
)

constraint_value(
    name = "ppc64le",
    constraint_setting = ":cpu",
)

constraint_value(
    name = "s390x",
    constraint_setting = ":cpu",
)

constraint_value(
    name = "x86_32",
    constraint_setting = ":cpu",
)

constraint_value(
    name = "x86_64",
    constraint_setting = ":cpu",
)

constraint_value(
    name = "wasm32",
    constraint_setting = ":cpu",
)

constraint_value(
    name = "wasm64",
    constraint_setting = ":cpu",
)

constraint_value(
    name = "mips64",
    constraint_setting = ":cpu",
)

constraint_value(
    name = "riscv32",
    constraint_setting = ":cpu",
)

constraint_value(
    name = "riscv64",
    constraint_setting = ":cpu",
)
