package(default_visibility = ["//visibility:public"])

# Allows toolchains and targets to constrain themselves based on Rust release
# channels, for example when depending on features only available in Nightly.
#
# https://doc.rust-lang.org/book/appendix-07-nightly-rust.html
constraint_setting(name = "channel")

constraint_value(
    name = "beta",
    constraint_setting = ":channel",
)

constraint_value(
    name = "nightly",
    constraint_setting = ":channel",
)

constraint_value(
    name = "stable",
    constraint_setting = ":channel",
)
