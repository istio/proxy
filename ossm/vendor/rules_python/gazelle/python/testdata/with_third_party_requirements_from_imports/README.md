# With third-party requirements (from imports)

This test case covers imports of the form:

```python
from my_pip_dep import foo
```

for example

```python
from google.cloud import aiplatform, storage
```

See https://github.com/bazel-contrib/rules_python/issues/709 and https://github.com/sramirezmartin/gazelle-toy-example.
