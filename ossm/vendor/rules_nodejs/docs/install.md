---
title: Installation
layout: default
toc: true
---

# Installation

## Installation with a specific version of Node.js

You can choose a specific version of Node.js. We mirror all published versions, which you can see in this repo at `/nodejs/private/node_versions.bzl`.

> Now that Node 12 is LTS (Long-term support) we encourage you to upgrade, and don't intend to fix bugs which are only observed in Node 10 or lower.
> Some of our packages have started to use features from Node 12, so you may see warnings if you use an older version.

Add to `WORKSPACE`:

```python
nodejs_repositories(
    node_version = "8.11.1",
)
```

## Installation with a manually specified version of Node.js

If you'd like to use a version of Node.js that is not currently supported here,
for example one that you host within your org, you can manually specify those in your `WORKSPACE`:

```python
load("@rules_nodejs//nodejs:repositories.bzl", "node_repositories")

nodejs_repositories(
  node_version = "8.10.0",
  node_repositories = {
    "8.10.0-darwin_amd64": ("node-v8.10.0-darwin-x64.tar.gz", "node-v8.10.0-darwin-x64", "7d77bd35bc781f02ba7383779da30bd529f21849b86f14d87e097497671b0271"),
    "8.10.0-linux_amd64": ("node-v8.10.0-linux-x64.tar.xz", "node-v8.10.0-linux-x64", "92220638d661a43bd0fee2bf478cb283ead6524f231aabccf14c549ebc2bc338"),
    "8.10.0-windows_amd64": ("node-v8.10.0-win-x64.zip", "node-v8.10.0-win-x64", "936ada36cb6f09a5565571e15eb8006e45c5a513529c19e21d070acf0e50321b"),
  },
  node_urls = ["https://nodejs.org/dist/v{version}/{filename}"],
```

Specifying `node_urls` is optional. If omitted, the default values will be used.

## Installation with local vendored versions of Node.js

You can use your own Node.js binary rather than fetching from the internet.
You could check in a binary file, or build Node.js from sources.
To use See [`nodejs_toolchain`](./Core.md#nodejs_toolchain) for docs.
