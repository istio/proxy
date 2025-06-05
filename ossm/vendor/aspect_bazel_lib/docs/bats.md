<!-- Generated with Stardoc: http://skydoc.bazel.build -->

A test rule that invokes the [Bash Automated Testing System](https://github.com/bats-core/bats-core).

For example, a `bats_test` target containing a single .bat and basic configuration:

```starlark
bats_test(
    name = "my_test",
    size = "small",
    srcs = [
        "my_test.bats",
    ],
    data = [
        "data.bin",
    ],
    env = {
        "DATA_PATH": "$(location :data.bin)",
    },
    args = ["--timing"],
)
```

<a id="bats_test"></a>

## bats_test

<pre>
bats_test(<a href="#bats_test-name">name</a>, <a href="#bats_test-srcs">srcs</a>, <a href="#bats_test-data">data</a>, <a href="#bats_test-env">env</a>)
</pre>



**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="bats_test-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="bats_test-srcs"></a>srcs |  Test files   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="bats_test-data"></a>data |  Runtime dependencies of the test.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="bats_test-env"></a>env |  Environment variables of the action.<br><br>Subject to [$(location)](https://bazel.build/reference/be/make-variables#predefined_label_variables) and ["Make variable"](https://bazel.build/reference/be/make-variables) substitution.   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional |  `{}`  |


