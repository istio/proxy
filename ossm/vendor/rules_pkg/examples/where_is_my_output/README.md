# Where are my files?

## Finding the exact path to the files created by a target.

Most of the time, Bazel users do not need to know the path to the artifacts
created for any given target. A notable exception is for users of packaging
rules. You typically create an RPM or Debian packaged file for the explicit
purpose of taking it from your machine and giving it to someone else.

Users often create scripts to push `bazel build` outputs to other places and
need to know the path to those outputs. This can be a challenge for rules which
may create the file name by combining a base part with a version number,
and maybe a CPU architecture. We don't do find them with shell wildcards
like `bazel-bin/my-pkg/pkg-*.deb`. That is brittle. Fortunately, Bazel
provide all the tools we need to get the precise path of an output.

## Using cquery to find the exact path to the outputs created for a target.

We can use Bazel's cquery command to find information about a target.

In Bazel 5.3.0 or later, there is a [`--output=files` flag](https://bazel.build/query/cquery#files-output) that provides this info directly. ([PR](https://github.com/bazelbuild/bazel/pull/15552))

Older versions of Bazel require a small Starlark program to be supplied.
Specifically we use
[cquery's Starlark output](https://docs.bazel.build/versions/main/cquery.html#cquery-starlark-dialect)
to inspect a target and print exactly what we need. Let's try it:

```shell
bazel build :deb
bazel cquery :deb --output=starlark --starlark:file=show_all_outputs.bzl 2>/dev/null
```

That should produce something like

```python
deb: bazel-out/k8-fastbuild/bin/mwp_3.14_all.deb
changes: bazel-out/k8-fastbuild/bin/mwp_3.changes
```

### How it works

show_all_outputs.bzl is a Starlark script that must contain a function with the
name `format`, that takes a single argument. The argument is typically named
target, and is a configured Bazel target, as you might have access to while
writing a custom rule. We can inspect its providers and print them in a useful
way.

For pkg_deb, there are two files, the .deb file and the .changes, and both are
passed along in the rule's OutputGroupInfo provider. This snippet below (from
show_all_outputs.bzl) prints them.

```python
def format(target):
    provider_map = providers(target)
    output_group_info = provider_map["OutputGroupInfo"]
    # Look at the attributes of the provider. Visit the depsets.
    ret = []
    for attr in dir(output_group_info):
        if attr.startswith("_"):
            continue
        attr_value = getattr(output_group_info, attr)
        if type(attr_value) == "depset":
            for file in attr_value.to_list():
                ret.append("%s: %s" % (attr, file.path))
    return "\n".join(ret)
```

A full explanation of why this works is beyond the scope of this example. It
requires some knowledge of how to write custom Bazel rules. See the Bazel
documentation for more information.

## Using an implicit output as input to another rule.

Sometimes a rule will create an implicit output that the user does not
explicitly specify as an attribute of the target. The .changes file from
pkg_deb is an example. If we want another rule to depend on an implicitly
created file, we can do that with a filegroup that specifies the specific
output group containing that file.

In the example below, `:deb` is a rule producing an explicit .deb output
and an implicit .changes output. We refer to the .changes file using the
`filegroup` and specifying the desired output group name. Then, any rule
can use this `filegroup` as an input.

```python

pkg_deb(name = "deb", ...)

filegroup(
    name = "the_changes_file",
    srcs = [":deb"],
    output_group = "changes",
)

genrule(
    name = "use_changes_file",
    srcs = [":the_changes_file"],
    outs = ["copy_of_changes.txt"],
    cmd = "cp $(location :the_changes_file) $@",
)
```
