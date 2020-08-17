# Extension server tool

This is a tool to apply configuration using ExtensionConfigDiscovery mechanism in Envoy.
The tool allows rapid pushes of Wasm modules and Wasm configuration. It accepts two flags:

- `-p` for the port to listen on.
- `-c` for the directory to watch configuration files for.

An example configuration is provided [here](/tools/extensionserver/testdata/stats.yaml).
Each extension configuration has the following fields:

- `name` is the name of the resource in ECDS.
- `path` or `url` for the Wasm code.
- `sha256` for the Wasm code SHA256 hash. The code is not refetched if the hash
  matches. The config is rejected if SHA256 is specified but does not match.
- `vm_id` and `root_id` optional settings for the VM and root context.
- `configuration` JSON configuration to apply for the extension.

## Usage

