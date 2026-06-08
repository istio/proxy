"""A test rule that invokes the [Bash Automated Testing System](https://github.com/bats-core/bats-core).

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
"""

load("//lib/private:bats.bzl", _bats_test = "bats_test")

bats_test = _bats_test
