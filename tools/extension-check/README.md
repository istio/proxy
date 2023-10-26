# Extension check

This is a simple tool to check all core extensions from envoy are enabled in [istio/proxy](https://github.com/istio/proxy).

## Usage

```bash
go run tools/extension-check/main.go \
  --ignore-extensions "<WELLKNOWN_IGNORE_EXTENSIONS>" \
  --envoy-extensions-build-config "<ENVOY_EXTENSIONS_BUILD_CONFIG>" \
  --proxy-extensions-build-config "<PROXY_EXTENSIONS_BUILD_CONFIG>"
```