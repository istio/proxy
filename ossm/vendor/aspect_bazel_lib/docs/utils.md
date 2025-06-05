<!-- Generated with Stardoc: http://skydoc.bazel.build -->

General-purpose Starlark utility functions

## Usage example

```starlark
load("@aspect_bazel_lib//lib:utils.bzl", "utils")

out_label = utils.to_label(out_file)
```

<a id="consistent_label_str"></a>

## consistent_label_str

<pre>
consistent_label_str(<a href="#consistent_label_str-ctx">ctx</a>, <a href="#consistent_label_str-label">label</a>)
</pre>

Generate a consistent label string for all Bazel versions.

Starting in Bazel 6, the workspace name is empty for the local workspace and there's no other
way to determine it. This behavior differs from Bazel 5 where the local workspace name was fully
qualified in str(label).

This utility function is meant for use in rules and requires the rule context to determine the
user's workspace name (`ctx.workspace_name`).


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="consistent_label_str-ctx"></a>ctx |  The rule context.   |  none |
| <a id="consistent_label_str-label"></a>label |  A Label.   |  none |

**RETURNS**

String representation of the label including the repository name if the label is from an
  external repository. For labels in the user's repository the label will start with `@//`.


<a id="default_timeout"></a>

## default_timeout

<pre>
default_timeout(<a href="#default_timeout-size">size</a>, <a href="#default_timeout-timeout">timeout</a>)
</pre>

Provide a sane default for *_test timeout attribute.

The [test-encyclopedia](https://bazel.build/reference/test-encyclopedia) says:

> Tests may return arbitrarily fast regardless of timeout.
> A test is not penalized for an overgenerous timeout, although a warning may be issued:
> you should generally set your timeout as tight as you can without incurring any flakiness.

However Bazel's default for timeout is medium, which is dumb given this guidance.

It also says:

> Tests which do not explicitly specify a timeout have one implied based on the test's size as follows

Therefore if size is specified, we should allow timeout to take its implied default.
If neither is set, then we can fix Bazel's wrong default here to avoid warnings under
`--test_verbose_timeout_warnings`.

This function can be used in a macro which wraps a testing rule.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="default_timeout-size"></a>size |  the size attribute of a test target   |  none |
| <a id="default_timeout-timeout"></a>timeout |  the timeout attribute of a test target   |  none |

**RETURNS**

"short" if neither is set, otherwise timeout


<a id="file_exists"></a>

## file_exists

<pre>
file_exists(<a href="#file_exists-path">path</a>)
</pre>

Check whether a file exists.

Useful in macros to set defaults for a configuration file if it is present.
This can only be called during the loading phase, not from a rule implementation.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="file_exists-path"></a>path |  a label, or a string which is a path relative to this package   |  none |


<a id="glob_directories"></a>

## glob_directories

<pre>
glob_directories(<a href="#glob_directories-include">include</a>, <a href="#glob_directories-kwargs">kwargs</a>)
</pre>



**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="glob_directories-include"></a>include |  <p align="center"> - </p>   |  none |
| <a id="glob_directories-kwargs"></a>kwargs |  <p align="center"> - </p>   |  none |


<a id="is_bazel_6_or_greater"></a>

## is_bazel_6_or_greater

<pre>
is_bazel_6_or_greater()
</pre>

Detects if the Bazel version being used is greater than or equal to 6 (including Bazel 6 pre-releases and RCs).

Detecting Bazel 6 or greater is particularly useful in rules as slightly different code paths may be needed to
support bzlmod which was added in Bazel 6.

Unlike the undocumented `native.bazel_version`, which only works in WORKSPACE and repository rules, this function can
be used in rules and BUILD files.

An alternate approach to make the Bazel version available in BUILD files and rules would be to
use the [host_repo](https://github.com/bazel-contrib/bazel-lib/blob/main/docs/host_repo.md) repository rule
which contains the bazel_version in the exported `host` struct:

WORKSPACE:
```
load("@aspect_bazel_lib//lib:host_repo.bzl", "host_repo")
host_repo(name = "aspect_bazel_lib_host")
```

BUILD.bazel:
```
load("@aspect_bazel_lib_host//:defs.bzl", "host")
print(host.bazel_version)
```

That approach, however, incurs a cost in the user's WORKSPACE.



**RETURNS**

True if the Bazel version being used is greater than or equal to 6 (including pre-releases and RCs)


<a id="is_bazel_7_or_greater"></a>

## is_bazel_7_or_greater

<pre>
is_bazel_7_or_greater()
</pre>

Detects if the Bazel version being used is greater than or equal to 7 (including Bazel 7 pre-releases and RCs).

Unlike the undocumented `native.bazel_version`, which only works in WORKSPACE and repository rules, this function can
be used in rules and BUILD files.

An alternate approach to make the Bazel version available in BUILD files and rules would be to
use the [host_repo](https://github.com/bazel-contrib/bazel-lib/blob/main/docs/host_repo.md) repository rule
which contains the bazel_version in the exported `host` struct:

WORKSPACE:
```
load("@aspect_bazel_lib//lib:host_repo.bzl", "host_repo")
host_repo(name = "aspect_bazel_lib_host")
```

BUILD.bazel:
```
load("@aspect_bazel_lib_host//:defs.bzl", "host")
print(host.bazel_version)
```

That approach, however, incurs a cost in the user's WORKSPACE.



**RETURNS**

True if the Bazel version being used is greater than or equal to 7 (including pre-releases and RCs)


<a id="is_bzlmod_enabled"></a>

## is_bzlmod_enabled

<pre>
is_bzlmod_enabled()
</pre>

Detect the value of the --enable_bzlmod flag



<a id="is_external_label"></a>

## is_external_label

<pre>
is_external_label(<a href="#is_external_label-param">param</a>)
</pre>

Returns True if the given Label (or stringy version of a label) represents a target outside of the workspace

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="is_external_label-param"></a>param |  a string or label   |  none |

**RETURNS**

a bool


<a id="maybe_http_archive"></a>

## maybe_http_archive

<pre>
maybe_http_archive(<a href="#maybe_http_archive-kwargs">kwargs</a>)
</pre>

Adapts a maybe(http_archive, ...) to look like an http_archive.

This makes WORKSPACE dependencies easier to read and update.

Typical usage looks like,

```
load("//lib:utils.bzl", http_archive = "maybe_http_archive")

http_archive(
    name = "aspect_rules_js",
    sha256 = "5bb643d9e119832a383e67f946dc752b6d719d66d1df9b46d840509ceb53e1f1",
    strip_prefix = "rules_js-1.6.2",
    url = "https://github.com/aspect-build/rules_js/archive/refs/tags/v1.6.2.tar.gz",
)
```

instead of the classic maybe pattern of,

```
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

maybe(
    http_archive,
    name = "aspect_rules_js",
    sha256 = "5bb643d9e119832a383e67f946dc752b6d719d66d1df9b46d840509ceb53e1f1",
    strip_prefix = "rules_js-1.6.2",
    url = "https://github.com/aspect-build/rules_js/archive/refs/tags/v1.6.2.tar.gz",
)
```


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="maybe_http_archive-kwargs"></a>kwargs |  all arguments to pass-forward to http_archive   |  none |


<a id="path_to_workspace_root"></a>

## path_to_workspace_root

<pre>
path_to_workspace_root()
</pre>

Returns the path to the workspace root under bazel


**RETURNS**

Path to the workspace root


<a id="propagate_common_binary_rule_attributes"></a>

## propagate_common_binary_rule_attributes

<pre>
propagate_common_binary_rule_attributes(<a href="#propagate_common_binary_rule_attributes-attrs">attrs</a>)
</pre>

Returns a dict of rule parameters filtered from the input dict that only contains the ones that are common to all binary rules

These are listed in Bazel's documentation:
https://bazel.build/reference/be/common-definitions#common-attributes
https://bazel.build/reference/be/common-definitions#common-attributes-binary


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="propagate_common_binary_rule_attributes-attrs"></a>attrs |  Dict of parameters to filter   |  none |

**RETURNS**

The dict of parameters, containing only common binary attributes


<a id="propagate_common_rule_attributes"></a>

## propagate_common_rule_attributes

<pre>
propagate_common_rule_attributes(<a href="#propagate_common_rule_attributes-attrs">attrs</a>)
</pre>

Returns a dict of rule parameters filtered from the input dict that only contains the ones that are common to all rules

These are listed in Bazel's documentation:
https://bazel.build/reference/be/common-definitions#common-attributes


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="propagate_common_rule_attributes-attrs"></a>attrs |  Dict of parameters to filter   |  none |

**RETURNS**

The dict of parameters, containing only common attributes


<a id="propagate_common_test_rule_attributes"></a>

## propagate_common_test_rule_attributes

<pre>
propagate_common_test_rule_attributes(<a href="#propagate_common_test_rule_attributes-attrs">attrs</a>)
</pre>

Returns a dict of rule parameters filtered from the input dict that only contains the ones that are common to all test rules

These are listed in Bazel's documentation:
https://bazel.build/reference/be/common-definitions#common-attributes
https://bazel.build/reference/be/common-definitions#common-attributes-tests


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="propagate_common_test_rule_attributes-attrs"></a>attrs |  Dict of parameters to filter   |  none |

**RETURNS**

The dict of parameters, containing only common test attributes


<a id="propagate_well_known_tags"></a>

## propagate_well_known_tags

<pre>
propagate_well_known_tags(<a href="#propagate_well_known_tags-tags">tags</a>)
</pre>

Returns a list of tags filtered from the input set that only contains the ones that are considered "well known"

These are listed in Bazel's documentation:
https://docs.bazel.build/versions/main/test-encyclopedia.html#tag-conventions
https://docs.bazel.build/versions/main/be/common-definitions.html#common-attributes


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="propagate_well_known_tags-tags"></a>tags |  List of tags to filter   |  `[]` |

**RETURNS**

List of tags that only contains the well known set


<a id="to_label"></a>

## to_label

<pre>
to_label(<a href="#to_label-param">param</a>)
</pre>

Converts a string to a Label. If Label is supplied, the same label is returned.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="to_label-param"></a>param |  a string representing a label or a Label   |  none |

**RETURNS**

a Label


<a id="utils.consistent_label_str"></a>

## utils.consistent_label_str

<pre>
utils.consistent_label_str(<a href="#utils.consistent_label_str-ctx">ctx</a>, <a href="#utils.consistent_label_str-label">label</a>)
</pre>

Generate a consistent label string for all Bazel versions.

Starting in Bazel 6, the workspace name is empty for the local workspace and there's no other
way to determine it. This behavior differs from Bazel 5 where the local workspace name was fully
qualified in str(label).

This utility function is meant for use in rules and requires the rule context to determine the
user's workspace name (`ctx.workspace_name`).


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="utils.consistent_label_str-ctx"></a>ctx |  The rule context.   |  none |
| <a id="utils.consistent_label_str-label"></a>label |  A Label.   |  none |

**RETURNS**

String representation of the label including the repository name if the label is from an
  external repository. For labels in the user's repository the label will start with `@//`.


<a id="utils.default_timeout"></a>

## utils.default_timeout

<pre>
utils.default_timeout(<a href="#utils.default_timeout-size">size</a>, <a href="#utils.default_timeout-timeout">timeout</a>)
</pre>

Provide a sane default for *_test timeout attribute.

The [test-encyclopedia](https://bazel.build/reference/test-encyclopedia) says:

> Tests may return arbitrarily fast regardless of timeout.
> A test is not penalized for an overgenerous timeout, although a warning may be issued:
> you should generally set your timeout as tight as you can without incurring any flakiness.

However Bazel's default for timeout is medium, which is dumb given this guidance.

It also says:

> Tests which do not explicitly specify a timeout have one implied based on the test's size as follows

Therefore if size is specified, we should allow timeout to take its implied default.
If neither is set, then we can fix Bazel's wrong default here to avoid warnings under
`--test_verbose_timeout_warnings`.

This function can be used in a macro which wraps a testing rule.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="utils.default_timeout-size"></a>size |  the size attribute of a test target   |  none |
| <a id="utils.default_timeout-timeout"></a>timeout |  the timeout attribute of a test target   |  none |

**RETURNS**

"short" if neither is set, otherwise timeout


<a id="utils.file_exists"></a>

## utils.file_exists

<pre>
utils.file_exists(<a href="#utils.file_exists-path">path</a>)
</pre>

Check whether a file exists.

Useful in macros to set defaults for a configuration file if it is present.
This can only be called during the loading phase, not from a rule implementation.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="utils.file_exists-path"></a>path |  a label, or a string which is a path relative to this package   |  none |


<a id="utils.glob_directories"></a>

## utils.glob_directories

<pre>
utils.glob_directories(<a href="#utils.glob_directories-include">include</a>, <a href="#utils.glob_directories-kwargs">kwargs</a>)
</pre>



**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="utils.glob_directories-include"></a>include |  <p align="center"> - </p>   |  none |
| <a id="utils.glob_directories-kwargs"></a>kwargs |  <p align="center"> - </p>   |  none |


<a id="utils.is_bazel_6_or_greater"></a>

## utils.is_bazel_6_or_greater

<pre>
utils.is_bazel_6_or_greater()
</pre>

Detects if the Bazel version being used is greater than or equal to 6 (including Bazel 6 pre-releases and RCs).

Detecting Bazel 6 or greater is particularly useful in rules as slightly different code paths may be needed to
support bzlmod which was added in Bazel 6.

Unlike the undocumented `native.bazel_version`, which only works in WORKSPACE and repository rules, this function can
be used in rules and BUILD files.

An alternate approach to make the Bazel version available in BUILD files and rules would be to
use the [host_repo](https://github.com/bazel-contrib/bazel-lib/blob/main/docs/host_repo.md) repository rule
which contains the bazel_version in the exported `host` struct:

WORKSPACE:
```
load("@aspect_bazel_lib//lib:host_repo.bzl", "host_repo")
host_repo(name = "aspect_bazel_lib_host")
```

BUILD.bazel:
```
load("@aspect_bazel_lib_host//:defs.bzl", "host")
print(host.bazel_version)
```

That approach, however, incurs a cost in the user's WORKSPACE.



**RETURNS**

True if the Bazel version being used is greater than or equal to 6 (including pre-releases and RCs)


<a id="utils.is_bazel_7_or_greater"></a>

## utils.is_bazel_7_or_greater

<pre>
utils.is_bazel_7_or_greater()
</pre>

Detects if the Bazel version being used is greater than or equal to 7 (including Bazel 7 pre-releases and RCs).

Unlike the undocumented `native.bazel_version`, which only works in WORKSPACE and repository rules, this function can
be used in rules and BUILD files.

An alternate approach to make the Bazel version available in BUILD files and rules would be to
use the [host_repo](https://github.com/bazel-contrib/bazel-lib/blob/main/docs/host_repo.md) repository rule
which contains the bazel_version in the exported `host` struct:

WORKSPACE:
```
load("@aspect_bazel_lib//lib:host_repo.bzl", "host_repo")
host_repo(name = "aspect_bazel_lib_host")
```

BUILD.bazel:
```
load("@aspect_bazel_lib_host//:defs.bzl", "host")
print(host.bazel_version)
```

That approach, however, incurs a cost in the user's WORKSPACE.



**RETURNS**

True if the Bazel version being used is greater than or equal to 7 (including pre-releases and RCs)


<a id="utils.is_bzlmod_enabled"></a>

## utils.is_bzlmod_enabled

<pre>
utils.is_bzlmod_enabled()
</pre>

Detect the value of the --enable_bzlmod flag



<a id="utils.is_external_label"></a>

## utils.is_external_label

<pre>
utils.is_external_label(<a href="#utils.is_external_label-param">param</a>)
</pre>

Returns True if the given Label (or stringy version of a label) represents a target outside of the workspace

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="utils.is_external_label-param"></a>param |  a string or label   |  none |

**RETURNS**

a bool


<a id="utils.maybe_http_archive"></a>

## utils.maybe_http_archive

<pre>
utils.maybe_http_archive(<a href="#utils.maybe_http_archive-kwargs">kwargs</a>)
</pre>

Adapts a maybe(http_archive, ...) to look like an http_archive.

This makes WORKSPACE dependencies easier to read and update.

Typical usage looks like,

```
load("//lib:utils.bzl", http_archive = "maybe_http_archive")

http_archive(
    name = "aspect_rules_js",
    sha256 = "5bb643d9e119832a383e67f946dc752b6d719d66d1df9b46d840509ceb53e1f1",
    strip_prefix = "rules_js-1.6.2",
    url = "https://github.com/aspect-build/rules_js/archive/refs/tags/v1.6.2.tar.gz",
)
```

instead of the classic maybe pattern of,

```
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

maybe(
    http_archive,
    name = "aspect_rules_js",
    sha256 = "5bb643d9e119832a383e67f946dc752b6d719d66d1df9b46d840509ceb53e1f1",
    strip_prefix = "rules_js-1.6.2",
    url = "https://github.com/aspect-build/rules_js/archive/refs/tags/v1.6.2.tar.gz",
)
```


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="utils.maybe_http_archive-kwargs"></a>kwargs |  all arguments to pass-forward to http_archive   |  none |


<a id="utils.path_to_workspace_root"></a>

## utils.path_to_workspace_root

<pre>
utils.path_to_workspace_root()
</pre>

Returns the path to the workspace root under bazel


**RETURNS**

Path to the workspace root


<a id="utils.propagate_common_binary_rule_attributes"></a>

## utils.propagate_common_binary_rule_attributes

<pre>
utils.propagate_common_binary_rule_attributes(<a href="#utils.propagate_common_binary_rule_attributes-attrs">attrs</a>)
</pre>

Returns a dict of rule parameters filtered from the input dict that only contains the ones that are common to all binary rules

These are listed in Bazel's documentation:
https://bazel.build/reference/be/common-definitions#common-attributes
https://bazel.build/reference/be/common-definitions#common-attributes-binary


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="utils.propagate_common_binary_rule_attributes-attrs"></a>attrs |  Dict of parameters to filter   |  none |

**RETURNS**

The dict of parameters, containing only common binary attributes


<a id="utils.propagate_common_rule_attributes"></a>

## utils.propagate_common_rule_attributes

<pre>
utils.propagate_common_rule_attributes(<a href="#utils.propagate_common_rule_attributes-attrs">attrs</a>)
</pre>

Returns a dict of rule parameters filtered from the input dict that only contains the ones that are common to all rules

These are listed in Bazel's documentation:
https://bazel.build/reference/be/common-definitions#common-attributes


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="utils.propagate_common_rule_attributes-attrs"></a>attrs |  Dict of parameters to filter   |  none |

**RETURNS**

The dict of parameters, containing only common attributes


<a id="utils.propagate_common_test_rule_attributes"></a>

## utils.propagate_common_test_rule_attributes

<pre>
utils.propagate_common_test_rule_attributes(<a href="#utils.propagate_common_test_rule_attributes-attrs">attrs</a>)
</pre>

Returns a dict of rule parameters filtered from the input dict that only contains the ones that are common to all test rules

These are listed in Bazel's documentation:
https://bazel.build/reference/be/common-definitions#common-attributes
https://bazel.build/reference/be/common-definitions#common-attributes-tests


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="utils.propagate_common_test_rule_attributes-attrs"></a>attrs |  Dict of parameters to filter   |  none |

**RETURNS**

The dict of parameters, containing only common test attributes


<a id="utils.propagate_well_known_tags"></a>

## utils.propagate_well_known_tags

<pre>
utils.propagate_well_known_tags(<a href="#utils.propagate_well_known_tags-tags">tags</a>)
</pre>

Returns a list of tags filtered from the input set that only contains the ones that are considered "well known"

These are listed in Bazel's documentation:
https://docs.bazel.build/versions/main/test-encyclopedia.html#tag-conventions
https://docs.bazel.build/versions/main/be/common-definitions.html#common-attributes


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="utils.propagate_well_known_tags-tags"></a>tags |  List of tags to filter   |  `[]` |

**RETURNS**

List of tags that only contains the well known set


<a id="utils.to_label"></a>

## utils.to_label

<pre>
utils.to_label(<a href="#utils.to_label-param">param</a>)
</pre>

Converts a string to a Label. If Label is supplied, the same label is returned.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="utils.to_label-param"></a>param |  a string representing a label or a Label   |  none |

**RETURNS**

a Label


