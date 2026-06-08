# [Bazel Platforms](https://bazel.build)

This repository houses all canonical
[constraint_setting()](https://bazel.build/reference/be/platforms-and-toolchains#constraint_setting)s,
[constraint_value()](https://bazel.build/reference/be/platforms-and-toolchains#constraint_value)s
and
[platform()](https://bazel.build/reference/be/platforms-and-toolchains#platform)s
that are universally useful across languages and Bazel projects.

For questions or concerns please email
[bazel-discuss@googlegroups.com](mailto://bazel-discuss@googlegroups.com).

# Motivation

Constraints must be carefully organized to avoid fragmentation. If two different
declarations for, say, cpu=x86_64 were to exist at the same time then select()
statements and toolchain selection would stop working across languages and
projects.

# Process

This repository only includes truly ubiquitous constraints.

Most common constraints -- that is settings and values that can be used across
projects - fall into specific areas like "Apple" or "Java". These are declared
in those areas' respective repositories.

A very few constraints (such as OS and cpu) are relevant for essentially all
projects across all areas. These are what this repository is for.

# Adding a canonical constraint value

To add a new canonical constraint value, prepare a PR adding it to that the
appropriate BUILD file.

Note that even global constraint values are typically area values. For example,
ios is an area value for the global setting os but belongs in the apple area.
For the PR's reviewer(s) choose an owner of this repository plus an owner of the
area repository this references..

A constraint value should be:

-   semantically clear, particularly in its distinctions from other values of
    the same setting

-   well-named: consistent with existing values of the same setting and easy to
    understand at usage sites

-   well-documented

Remember that this value will apply for the entire Bazel community and its
semantics will be difficult to impossible to change once it starts being used.

# Adding a canonical constraint setting

New canonical constraint settings should be rare and well-justified.

To add a new setting, prepare a design document according to Bazel's design
review process. This document should explain the need for a new setting and why
it belongs here vs. area-specific repositories. It should clearly explain
semantics, initial values, and criteria for adding new values.

Once the design is approved prepare a PR for the actual change. If any values
are area-specific, include the area repositories' owners as reviewers.

# Private changes to global constraints

If you'd like to experiment with changes to global settings or values, you can
fork this repo for experimental purposes. But in the interest of community
health and interoperability please don't share your changes with anyone not
involved with the experiment. For wider distribution, submit a proper change
here.

Note that you can declare constraint_values in your own repo that are members of
the global constraint_settings. This lets you "extend" global settings within
the confines of your own project. But don't do this if you expect other projects
to use these changes - this can easily lead to fragmentation conflicts.

If you need custom constaint_settings, just declare them in your own repo. They
are, by definition, not global.

If you really need a permanent global change and it isn't design-approved for
this repo, start a thread on
[GitHub](https://github.com/bazelbuild/bazel/discussions) to discuss options.
