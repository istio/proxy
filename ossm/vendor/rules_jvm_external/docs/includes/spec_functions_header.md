# Maven specification functions

These are helper functions to specify more information about Maven artifacts and
repositories in `maven_install`.

To use these functions, load the `maven` struct at the top of your BUILD file:

```python
load("@rules_jvm_external//:specs.bzl", "maven")
```
