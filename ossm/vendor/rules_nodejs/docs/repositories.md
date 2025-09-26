# Generated Repositories

rules_nodejs produces several repositories for you to reference.
Bazel represents your workspace as one repository, and code fetched or installed from outside your workspace lives in other repositories.
These are referenced with the `@repo//` syntax in your BUILD files.

## @nodejs

This repository is created by calling the `nodejs_repositories` function in your `WORKSPACE` file.
It contains the node, npm, and npx programs.

As always, `bazel query` is useful for learning about what targets are available.

```sh
$ bazel query @nodejs//...
@nodejs//:node
...
```

You don't typically need to reference the `@nodejs` repository from your BUILD files because it's used behind the scenes to run node and fetch dependencies.

Some ways you can use this:

- Run the Bazel-managed version of node: `bazel run @nodejs//:node path/to/program.js`
- Run the Bazel-managed version of npm: `bazel run @nodejs//:npm`

(Note: for backward-compatibility, the `@nodejs` repository can also be referenced as `@nodejs_host`).