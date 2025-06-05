"""# rules_rust_mdbook

Bazel rules for [mdBook](https://github.com/rust-lang/mdBook).

## Rules

- [mdbook](#mdbook)
- [mdbook_server](#mdbook_server)
- [mdbook_toolchain](#mdbook_toolchain)

## Setup

### bzlmod

```python
bazel_dep(name = "rules_rust_mdbook", version = "{SEE_RELEASE_NOTES}")
```

### WORKSPACE

```python
load("@rules_rust_mdbook//:repositories.bzl", "mdbook_register_toolchains", "rules_mdbook_dependencies")

rules_mdbook_dependencies()

mdbook_register_toolchains()

load("@rules_rust_mdbook//:repositories_transitive.bzl", "rules_mdbook_transitive_deps")

rules_mdbook_transitive_deps()
```

---
---
"""

load(
    "//private:mdbook.bzl",
    _mdbook = "mdbook",
    _mdbook_server = "mdbook_server",
)
load(
    "//private:toolchain.bzl",
    _mdbook_toolchain = "mdbook_toolchain",
)

mdbook = _mdbook
mdbook_server = _mdbook_server
mdbook_toolchain = _mdbook_toolchain
