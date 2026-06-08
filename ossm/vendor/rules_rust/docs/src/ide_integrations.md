# IDE Integrations

## VSCode

### Intellisense

The best intellisense integrations to date are documented for [rust-analyzer](./rust_analyzer.md). Please refer to this documentation for setup instructions.

### Debugging

`rules_rust` offers tooling to generate VSCode targets for running `rust_binary` and `rust_test` targets with a debugger in VSCode.

#### Prerequisites

Install [CodeLLDB](https://marketplace.visualstudio.com/items?itemName=vadimcn.vscode-lldb) extension in VSCode.

#### Generate Launch Configurations

Generate VSCode `launch.json` for debugging all Rust targets in the current workspace:

```bash
bazel run @rules_rust//tools/vscode:gen_launch_json
```

To scope debug generated `launch.json` targets, query patterns can be passed:

```bash
bazel run @rules_rust//tools/vscode:gen_launch_json -- //path/to/...
```

Bazel targets should now be available for debugging via the "Run and Debug" menu.
