"""Macro for creating sysroot build genrules."""

def sysrootfs(
        name,
        arch,
        glibc_version,
        debian_version,
        variant = "base",
        ppa_toolchain = None,
        stdcc_version = "13",
        tags = [
            "manual",
            "no-cache",
            "no-remote",
        ],
        extra_tags = [],
        visibility = ["//visibility:public"],
):
    """Create a genrule to build a sysroot.

    Args:
        name: Name of the genrule target
        arch: Architecture (amd64 or arm64)
        glibc_version: glibc version (e.g., "2.31" or "2.28")
        debian_version: Debian version (e.g., "bullseye" or "buster")
        variant: Sysroot variant ("base" or "libstdcxx")
        ppa_toolchain: Ubuntu PPA toolchain version (required for libstdcxx variant)
        stdcc_version: libstdc++ version (default: "13")
    """

    # Construct output filename
    if variant == "libstdcxx":
        output_file = "sysroot-glibc{}-libstdc++{}-{}.tar.xz".format(
            glibc_version,
            stdcc_version,
            arch,
        )
        build_args = "--arch {} --glibc {} --debian {} --variant {} --ppa-toolchain {} --stdcc {}".format(
            arch,
            glibc_version,
            debian_version,
            variant,
            ppa_toolchain,
            stdcc_version,
        )
    else:
        output_file = "sysroot-glibc{}-{}.tar.xz".format(
            glibc_version,
            arch,
        )
        build_args = "--arch {} --glibc {} --debian {} --variant {}".format(
            arch,
            glibc_version,
            debian_version,
            variant,
        )

    native.genrule(
        name = name,
        srcs = [":build_sysroot.sh"],
        outs = [output_file],
        cmd = """
        set -e
        SCRIPT=$(location :build_sysroot.sh)
        OUTPUT_DIR=$$(mktemp -d)
        (bash $$SCRIPT {} --workdir $$OUTPUT_DIR/sysroot-build --output $@  > log.txt 2>&1) \
        || (cat log.txt && exit 1)
        """.format(build_args),
        # Requires sudo, can't run in sandbox
        tags = [
            "no-sandbox",
        ] + tags + extra_tags,
        visibility = visibility,
        local = 1,
    )

def sysroots(arches, glibc, stdcc):
    """Generates matrix of sysroot targets.

    Args:
        arches: List of architectures (e.g. ["amd64", "arm64"])
        glibc: List of glibc versions (e.g. ["2.28", "2.31"])
        stdcc: List of stdcc versions (e.g. ["13", ""]).
               "" (empty string) implies variant="base".
               "13" implies variant="libstdcxx".
    """
    target_names = []

    for arch in arches:
        for glibc_version in glibc:
            # 1. Infer Debian version based on glibc
            if glibc_version == "2.31":
                debian_version = "bullseye"
            elif glibc_version == "2.28":
                debian_version = "buster"
            else:
                fail("Unsupported glibc version: {}".format(glibc_version))
            for cc_ver in stdcc:
                if cc_ver:
                    variant = "libstdcxx"
                    stdcc_version = cc_ver
                    ppa_toolchain = "focal" if debian_version == "bullseye" else "bionic"
                    name_suffix = "_libstdcxx"
                else:
                    variant = "base"
                    stdcc_version = None
                    ppa_toolchain = None
                    name_suffix = ""
                name = "sysroot_glibc{v}{s}_{a}".format(
                    v = glibc_version.replace(".", ""),
                    s = name_suffix,
                    a = arch,
                )
                sysrootfs(
                    name = name,
                    arch = arch,
                    glibc_version = glibc_version,
                    debian_version = debian_version,
                    variant = variant,
                    ppa_toolchain = ppa_toolchain,
                    stdcc_version = stdcc_version,
                )
                target_names.append(":" + name)

    native.filegroup(
        name = "sysroots",
        srcs = target_names,
        tags = ["manual"],
        visibility = ["//visibility:public"],
    )
