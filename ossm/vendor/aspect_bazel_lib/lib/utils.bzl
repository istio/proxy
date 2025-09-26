"""General-purpose Starlark utility functions

## Usage example

```starlark
load("@aspect_bazel_lib//lib:utils.bzl", "utils")

out_label = utils.to_label(out_file)
```

"""

load("//lib/private:utils.bzl", _utils = "utils")

utils = _utils

# TODO(3.0): remove these fine grained re-exports
default_timeout = _utils.default_timeout
file_exists = _utils.file_exists
glob_directories = _utils.glob_directories
is_bazel_6_or_greater = _utils.is_bazel_6_or_greater
is_bazel_7_or_greater = _utils.is_bazel_7_or_greater
is_bzlmod_enabled = _utils.is_bzlmod_enabled
is_external_label = _utils.is_external_label
maybe_http_archive = _utils.maybe_http_archive
path_to_workspace_root = _utils.path_to_workspace_root
propagate_well_known_tags = _utils.propagate_well_known_tags
propagate_common_rule_attributes = _utils.propagate_common_rule_attributes
propagate_common_test_rule_attributes = _utils.propagate_common_test_rule_attributes
propagate_common_binary_rule_attributes = _utils.propagate_common_binary_rule_attributes
to_label = _utils.to_label
consistent_label_str = _utils.consistent_label_str
# DON'T ADD ANY MORE fine-grained re-exports here; new util functions
# should just go through the 'utils' export above
