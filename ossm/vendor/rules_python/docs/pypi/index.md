:::{default-domain} bzl
:::

# Using PyPI

Using PyPI packages (aka "pip install") involves the following main steps:

1. [Generating requirements file](./lock)
2. Installing third-party packages in [bzlmod](./download) or [WORKSPACE](./download-workspace).
3. [Using third-party packages as dependencies](./use)

With the advanced topics covered separately:
* Dealing with [circular dependencies](./circular-dependencies).

```{toctree}
lock
download
download-workspace
use
```

## Advanced topics

```{toctree}
circular-dependencies
patch
```
