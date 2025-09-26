# Development guidelines

## Generate compilation database

[JSON Compilation Database](https://clang.llvm.org/docs/JSONCompilationDatabase.html) files can be used by [clangd](https://clangd.llvm.org/) or similar tools to add source code cross-references and code completion functionality to editors.

The following command can be used to generate the `compile_commands.json` file:

```
BAZEL_BUILD_OPTION_LIST="--define=engine=multi" ./tools/gen_compilation_database.py --include_all //test/... //:lib
```
