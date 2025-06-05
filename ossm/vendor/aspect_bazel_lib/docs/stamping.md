<!-- Generated with Stardoc: http://skydoc.bazel.build -->

# Version Stamping

Bazel is generally only a build tool, and is unaware of your version control system.
However, when publishing releases, you may want to embed version information in the resulting distribution.
Bazel supports this with the concept of a "Workspace status" which is evaluated before each build.
See [the Bazel workspace status docs](https://docs.bazel.build/versions/master/user-manual.html#workspace_status)

To stamp a build, you pass the `--stamp` argument to Bazel.

> Note: https://github.com/bazelbuild/bazel/issues/14341 proposes that Bazel enforce this by
> only giving constant values to rule implementations when stamping isn't enabled.

Stamping is typically performed on a later action in the graph, like on a linking or packaging rule (`pkg_*`).
This means that a changed status variable only causes that action, not re-compilation and thus does not cause cascading re-builds.

Bazel provides a couple of statuses by default, such as `BUILD_EMBED_LABEL` which is the value of the `--embed_label`
argument, as well as `BUILD_HOST`, `BUILD_TIMESTAMP`, and `BUILD_USER`.
You can supply more with the workspace status script, see below.

Some rules accept an attribute that uses the status variables.
They will usually say something like "subject to stamp variable replacements".

## Stamping with a Workspace status script

To define additional statuses, pass the `--workspace_status_command` flag to `bazel`.
This slows down every build, so you should avoid passing this flag unless you need to stamp this build.
The value of this flag is a path to a script that prints space-separated key/value pairs, one per line, such as

```bash
#!/usr/bin/env bash
echo STABLE_GIT_COMMIT $(git rev-parse HEAD)
```
> For a more full-featured script, take a look at this [example in Angular]

Make sure you set the executable bit, eg. `chmod +x tools/bazel_stamp_vars.sh`.

> **NOTE** keys that start with `STABLE_` will cause a re-build when they change.
> Other keys will NOT cause a re-build, so stale values can appear in your app.
> Non-stable (volatile) keys should typically be things like timestamps that always vary between builds.

You might like to encode your setup using an entry in `.bazelrc` such as:

```sh
# This tells Bazel how to interact with the version control system
# Enable this with --config=release
build:release --stamp --workspace_status_command=./tools/bazel_stamp_vars.sh
```

[example in Angular]: https://github.com/angular/angular/blob/df274b478e6597cb1a2f31bb9f599281065aa250/dev-infra/release/env-stamp.ts

## Writing a custom rule which reads stamp variables

First, load the helpers:

```starlark
load("@aspect_bazel_lib//lib:stamping.bzl", "STAMP_ATTRS", "maybe_stamp")
```

In your rule implementation, call the `maybe_stamp` function.
If it returns `None` then this build doesn't have stamping enabled.
Otherwise you can use the returned struct to access two files.

1. The `stable_status` file contains the keys which were prefixed with `STABLE_`, see above.
2. The `volatile_status` file contains the rest of the keys.

```starlark
def _rule_impl(ctx):
    args = ctx.actions.args()
    inputs = []
    stamp = maybe_stamp(ctx)
    if stamp:
        args.add("--volatile_status_file", stamp.volatile_status_file.path)
        args.add("--stable_status_file", stamp.stable_status_file.path)
        inputs.extend([stamp.volatile_status_file, stamp.stable_status_file])

    # ... call actions which parse the stamp files and do something with the values ...
```

Finally, in the declaration of the rule, include the `STAMP_ATTRS` to declare attributes
which are read by that `maybe_stamp` function above.

```starlark
my_stamp_aware_rule = rule(
    attrs = dict({
        # ... my attributes ...
    }, **STAMP_ATTRS),
)
```

<a id="maybe_stamp"></a>

## maybe_stamp

<pre>
maybe_stamp(<a href="#maybe_stamp-ctx">ctx</a>)
</pre>

Provide the bazel-out/stable_status.txt and bazel-out/volatile_status.txt files.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="maybe_stamp-ctx"></a>ctx |  The rule context   |  none |

**RETURNS**

If stamping is not enabled for this rule under the current build, returns None.
  Otherwise, returns a struct containing (volatile_status_file, stable_status_file) keys


