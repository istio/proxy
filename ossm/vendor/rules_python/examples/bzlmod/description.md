Before this PR the `coverage_tool` automatically registered by `rules_python`
was visible outside the toolchain repository. This fixes it to be consistent
with `non-bzlmod` setups and ensures that the default `coverage_tool` is not
visible outside the toolchain repos.

This means that the `MODULE.bazel` file can be cleaned-up at the expense of
relaxing the `coverage_tool` attribute for the `python_repository` to be a
simple string as the label would be evaluated within the context of
`rules_python` which may not necessarily resolve correctly without the
`use_repo` statement in our `MODULE.bazel`.
