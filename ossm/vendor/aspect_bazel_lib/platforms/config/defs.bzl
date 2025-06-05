"""This module generated the platforms list for the build matrix."""

platforms = [
    struct(os = os, cpu = cpu)
    for os in ["linux", "macos"]
    for cpu in ["aarch64", "x86_64"]
]
