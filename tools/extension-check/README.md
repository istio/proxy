# Extension check

This is a simple tool to check all core extensions from envoy are enabled in [istio/proxy](https://github.com/istio/proxy).

## Usage

```bash
go run tools/extension-check/main.go \
  --ignore-extensions "<WELLKNOWN_IGNORE_EXTENSIONS>" \
  --envoy-extensions-build-config "<ENVOY_EXTENSIONS_BUILD_CONFIG>" \
  --proxy-extensions-build-config "<PROXY_EXTENSIONS_BUILD_CONFIG>"
```

## Example

Envoy source code can be found at `~/Codes/istio.io/envoy`.
Proxy source code can be found at `~/Codes/istio.io/proxy`.

First you need delete the first line of proxy extensions build config file, which is `load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")`.

```bash
sed -i '1d' ~/Codes/istio.io/proxy/bazel/extension_config/extensions_build_config.bzl
```

Then you can run the following command to check all core extensions are enabled in proxy.

```bash
cd ~/Codes/istio.io/proxy
go run tools/extension-check/main.go \
  --ignore-extensions tools/extension-check/wellknown-extensions \
  --envoy-extensions-build-config "../envoy/source/extensions/extensions_build_config.bzl" \
  --proxy-extensions-build-config "./bazel/extension_config/extensions_build_config.bzl"
```
