"""Constants and utilities for platforms used for testing."""

platform_targets = struct(
    LINUX = Label("//tests/support/platforms:linux"),
    LINUX_AARCH64 = Label("//tests/support/platforms:linux_aarch64"),
    LINUX_X86_64 = Label("//tests/support/platforms:linux_x86_64"),
    MAC = Label("//tests/support/platforms:mac"),
    MAC_X86_64 = Label("//tests/support/platforms:mac_x86_64"),
    MAC_AARCH64 = Label("//tests/support/platforms:mac_aarch64"),
    WINDOWS = Label("//tests/support/platforms:windows"),
    WINDOWS_AARCH64 = Label("//tests/support/platforms:windows_aarch64"),
    WINDOWS_X86_64 = Label("//tests/support/platforms:windows_x86_64"),

    # Unspecified Unix platform that is unlikely to be the host platform in CI,
    # but still provides a Python toolchain.
    EXOTIC_UNIX = Label("//tests/support/platforms:exotic_unix"),
)
