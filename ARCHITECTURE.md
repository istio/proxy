# Architecture

This document describes the architecture of istio/proxy: what this repository is, how it relates to the broader Istio ecosystem, and how the build, extension, and test systems work.

See [CONTRIBUTING.md](CONTRIBUTING.md) for build setup and contribution guidelines.

## What This Repository Is

istio/proxy is not a standalone application. It is a thin build and configuration layer that produces the **Istio sidecar proxy binary** — the `istio-proxy` container that runs alongside every workload in sidecar mode.

The binary is Envoy. This repository:

1. Pins to a specific [envoyproxy/envoy](https://github.com/envoyproxy/envoy) commit via `ENVOY_SHA` in `MODULE.bazel`.
2. Selects the subset of Envoy extensions to compile into the binary.
3. Hosts Istio-specific extensions contributed upstream into Envoy's `contrib/istio/` directory.
4. Produces a binary and tarball that istiod configures at runtime via the xDS API.

## Relationship to Other Istio Components

```
┌─────────────────────────────────────────────────────┐
│                   Istio Control Plane                │
│  istiod  ──xDS (gRPC)──► istio-proxy (this binary) │
└─────────────────────────────────────────────────────┘
```

In **ambient mode**, ztunnel replaces the sidecar entirely — this binary is not involved. See [istio/ztunnel](https://github.com/istio/ztunnel).

| Component | Repository | Role |
|---|---|---|
| `istio-proxy` | **this repo** | Sidecar data-plane proxy (Envoy binary) |
| istiod | [istio/istio](https://github.com/istio/istio) | Control plane; configures proxy via xDS |
| ztunnel | [istio/ztunnel](https://github.com/istio/ztunnel) | Ambient mesh node proxy (Rust, not Envoy) |
| Envoy | [envoyproxy/envoy](https://github.com/envoyproxy/envoy) | Upstream proxy; provides all core functionality |

In sidecar mode, istiod pushes xDS resources (Listeners, Routes, Clusters, Endpoints) to the proxy. The proxy itself has no Istio-specific control-plane logic — that lives entirely in istiod.

## Repository Structure

```
proxy/
├── BUILD                          # Main Bazel target: produces the :envoy binary
├── MODULE.bazel                   # Bzlmod config; pins Envoy commit (ENVOY_SHA / ENVOY_SHA256)
├── ENVOY_VERSION.txt              # Envoy version string, synced from upstream on each bump
├── envoy.bazelrc                  # Bazel flags synced from envoyproxy/envoy
├── ci.bazelrc                     # CI-specific Bazel flags synced from upstream
├── bazel/
│   └── extension_config/
│       └── extensions_build_config.bzl  # Extension selection: which Envoy extensions compile in
├── test/
│   └── envoye2e/                  # Go integration tests that exercise the proxy end-to-end
│       ├── driver/                # Test framework: spins up real Envoy instances + xDS server
│       ├── basic_flow/            # Core HTTP/TCP proxy flow tests
│       ├── stats_plugin/          # Istio stats extension tests
│       ├── http_metadata_exchange/
│       ├── tcp_metadata_exchange/
│       └── workloadapi/           # Workload API (peer metadata) integration tests
├── scripts/
│   └── update_envoy.sh            # Bumps ENVOY_SHA and syncs related files from upstream
├── tools/
│   └── extension-check/           # Validates extension_build_config alignment with upstream
├── prow/                          # Prow CI job scripts (presubmit, postsubmit, release)
└── common/                        # Shared scripts from istio/common-files
```

## The Envoy Dependency

The entire Envoy source tree is consumed as a Bazel module dependency. `MODULE.bazel` pins it at a specific commit:

```python
ENVOY_SHA    = "<git-sha>"
ENVOY_SHA256 = "<sha256-of-tarball>"
ENVOY_ORG    = "envoyproxy"
ENVOY_REPO   = "envoy"

bazel_dep(name = "envoy", version = "1.40.0-dev")
archive_override(
    module_name = "envoy",
    sha256 = ENVOY_SHA256,
    strip_prefix = ENVOY_REPO + "-" + ENVOY_SHA,
    urls = ["https://github.com/" + ENVOY_ORG + "/" + ENVOY_REPO + "/archive/" + ENVOY_SHA + ".tar.gz"],
)
```

Envoy is not published to any Bzlmod registry, so `bazel_dep` alone cannot resolve it. The `archive_override` fetches the pinned GitHub archive directly. The same pattern repeats for `envoy_api` (Envoy's API subtree, which Bzlmod cannot derive from the `envoy` module override automatically).

To work against a local Envoy checkout, pass `--override_module=envoy=/PATH/TO/ENVOY --override_module=envoy_api=/PATH/TO/ENVOY/api` to Bazel, or persist those in `user.bazelrc`.

When Envoy moves forward, `scripts/update_envoy.sh` updates these files in a single run:

- `ENVOY_SHA` and `ENVOY_SHA256` in `MODULE.bazel`
- `ENVOY_VERSION.txt`
- `envoy.bazelrc` and `ci.bazelrc` (copied verbatim from upstream)
- `.bazelversion` (the Bazel version Envoy requires)
- `MODULE.bazel.lock` (re-generated so the lockfile stays consistent)

This is done automatically by the Istio automator bot, which accounts for most commits on `master`.

## Extension Model

Envoy's binary footprint is controlled at build time by a registry of extensions. This repo maintains that registry in `bazel/extension_config/extensions_build_config.bzl`.

### Extension selection

The file defines `ENVOY_EXTENSIONS`, a Starlark dict mapping extension names to Bazel targets:

```python
ENVOY_EXTENSIONS = {
    "envoy.filters.http.router":   "//source/extensions/filters/http/router:config",
    "envoy.filters.http.wasm":     "//source/extensions/filters/http/wasm:config",
    # ...
}
```

Extensions not listed here are excluded from the binary at compile time.

### Istio-specific extensions

Several extensions are not part of core Envoy — they live in Envoy's `contrib/istio/` directory and are developed by the Istio team:

| Extension | Purpose |
|---|---|
| `envoy.filters.http.peer_metadata` | Propagates workload metadata on HTTP connections |
| `envoy.filters.http.istio_stats` | Emits Istio traffic metrics (L7) |
| `envoy.filters.http.alpn` | Rewrites ALPN for Istio protocol detection |
| `envoy.filters.network.metadata_exchange` | Propagates metadata on TCP connections |
| `envoy.filters.network.peer_metadata` | Network-layer peer metadata (ambient mode support) |

### Extension policy

As of April 2024, **new extensions are not added to this repository** unless they are part of a core Istio API. Proposals for new extensions should go to [istio/istio](https://github.com/istio/istio/issues) or directly upstream to envoyproxy/envoy.

### Extension validation

`tools/extension-check/` is a Go tool run in CI that cross-checks `extensions_build_config.bzl` against Envoy's own extension registry, ensuring no core Envoy extension is accidentally omitted.

## Build System

The build uses [Bazel](https://bazel.build/) with [Bzlmod](https://bazel.build/external/bzlmod).

### Key targets

| Target | Description |
|---|---|
| `//:envoy` | The Istio proxy binary |
| `//:envoy_tar` | Binary packaged as a `tar.gz` under `/usr/local/bin/` |
| `//...` | All targets (used by `make build`) |

### Build configurations

| Make target | Bazel config | Use |
|---|---|---|
| `make build` | dev (default) | Local development |
| `make build_envoy` | `--config=release` | Release binary |
| `make build_envoy_asan` | `--config=clang-asan-ci` | Address sanitizer |
| `make build_envoy_tsan` | `--config=clang-tsan-ci` | Thread sanitizer |

On macOS, TSAN is not supported. ASAN uses `--config=macos-asan` instead.

### Build files synced from upstream

`envoy.bazelrc` and `ci.bazelrc` are copied verbatim from envoyproxy/envoy on each Envoy bump. Do not edit them manually — changes will be overwritten. Persistent local overrides belong in `.bazelrc.user` (gitignored).

## Testing

### Integration tests (`test/envoye2e/`)

The primary test suite spins up real Envoy processes and exercises them end-to-end. Written in Go, it uses a custom driver framework (`test/envoye2e/driver/`) that:

- Starts an in-process xDS server
- Launches the compiled Envoy binary
- Drives traffic through it and asserts on metrics, headers, and behavior

Run with:

```bash
make test
```

Key test areas:

| Package | What it tests |
|---|---|
| `basic_flow/` | Core HTTP and TCP proxy flows, CONNECT tunneling |
| `stats_plugin/` | Istio stats extension (metric emission, labels, ECDS) |
| `http_metadata_exchange/` | L7 workload metadata propagation |
| `tcp_metadata_exchange/` | L4 workload metadata propagation |
| `workloadapi/` | Workload API (peer metadata) discovery |

### Sanitizer tests

```bash
make test_asan   # Address sanitizer
make test_tsan   # Thread sanitizer (Linux only)
```

### Generated test data

`scripts/gen-testdata.sh` regenerates golden files under `testdata/`. Run `make gen` to update, `make gen-check` to verify in CI.

## CI

CI runs on [Prow](https://docs.prow.k8s.io/) via scripts in `prow/`:

| Script | Triggered by | What it runs |
|---|---|---|
| `proxy-presubmit.sh` | Pull requests | lint, gen-check, build, test |
| `proxy-presubmit-asan.sh` | Pull requests | ASAN build + test |
| `proxy-presubmit-tsan.sh` | Pull requests | TSAN build + test |
| `proxy-presubmit-wasm.sh` | Pull requests | Wasm build + test |
| `proxy-presubmit-release.sh` | Pull requests | Release build |
| `proxy-postsubmit.sh` | Merges to master | Release build + push |

## Further Reading

- [Envoy documentation](https://www.envoyproxy.io/docs) — runtime architecture, filter API, xDS protocol
- [Bazel Bzlmod reference](https://bazel.build/external/bzlmod) — module system used by this repo
- [Istio architecture overview](https://istio.io/latest/docs/ops/deployment/architecture/) — how istiod, the proxy, and ztunnel relate at the system level
- [Prow documentation](https://docs.prow.k8s.io/) — CI system running the jobs in `prow/`
