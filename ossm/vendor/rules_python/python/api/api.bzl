"""Public, analysis phase APIs for Python rules.

To use the analyis-time API, add the attributes to your rule, then
use `py_common.get()` to get the api object:

```
load("@rules_python//python/api:api.bzl", "py_common")

def _impl(ctx):
    py_api = py_common.get(ctx)

myrule = rule(
    implementation = _impl,
    attrs = {...} | py_common.API_ATTRS
)
```

:::{versionadded} 0.37.0
:::
"""

load("//python/private/api:api.bzl", _py_common = "py_common")

py_common = _py_common
