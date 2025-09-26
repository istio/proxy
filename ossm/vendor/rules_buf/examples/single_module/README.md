# Single `buf.yaml`

The example uses lint and breaking rules with a single module `buf.yaml`.

- The `buf.yaml` is exported in [BUILD](BUILD.bazel)
- [foo/v1](foo/v1/BUILD.bazel) contains lint rule that succeeds, it can be exeucted using

```sh
bazel test //foo/v1:foo_proto_lint
```

- [bar/v1](bar/v1/BUILD.bazel) contains lint rule that fails, it can be exeucted using

```sh
bazel test //bar/v1:bar_proto_lint
```

- [Build](BUILD.bazel) file contains a breaking change detection rule that succeeds, it can be executed using,

```sh
bazel test //:fooapis_proto_breaking
```
