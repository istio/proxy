# Common troubleshooting tips

## Module not found errors

This is the most common error rules_js users encounter.
These problems generally stem from a runtime `require` call of some library which was not declared as a dependency.

Fortunately, these problems are not unique to Bazel.
As described in [our documentation](./pnpm.md#hoisting),
rules_js should behave the same way `pnpm` does with [`hoist=false`](https://pnpm.io/npmrc#hoist).

These problems are also reproducible under [Yarn PnP](https://yarnpkg.com/features/pnp) because it
also relies on correct dependencies.

The Node.js documentation describes the algorithm used:
https://nodejs.org/api/modules.html#loading-from-node_modules-folders

Since the resolution starts from the callsite, the remedy depends on where the `require` statement appears.

### require appears in your code

This is the case when you write an `import` or `require` statement.

In this case you should add the runtime dependency to your BUILD file alongside your source file:

For example,

```starlark
js_library(
    name = "requires_foo",
    srcs = ["config.js"],          # contains "require('foo')"
    data = [":node_modules/foo"],  # satisfies that require
)
```

and also, the `foo` module should be listed in your `package.json#dependencies` since pnpm is strict
about hoisting transitive dependencies to the root of `node_modules`.

This case also includes when you run some other tool, passing it a `config.js` file.

> This is the "ideal" way for JavaScript tools to be configured, because it allows an easy
> "symmetry" where you `require` a library and declare your dependency on it in the same place.
> When you pass a tool a `config.json` or other non-JavaScript file, and have string-typed references
> to npm packages, you'll fall into the next case: "require appears in third-party code".

### require appears in third-party code

This case itself breaks down into three possible remedies, depending on whether you can move the
require to your own code, the missing dependency can be considered a "bug",
or the third-party package uses the "plugin pattern" to discover its
plugins dynamically at runtime based on finding them based on a string you provided.

#### The `require` can move to first-party

This is the most principled solution. In many cases, a library that accepts the name of a package as
a string will also accept it as an object, so you can refactor `config: ['some-package']` to
`config: [require('some-package')]`. You may need to change from json or yaml config to a JavaScript
config file to allow the `require` syntax.

Once you've done this, it's handled like the "require appears in your code" case above.

For example, the
[documentation for the postcss-loader for Webpack](https://webpack.js.org/loaders/postcss-loader/#sugarss)
suggests that you `npm install --save-dev sugarss`
and then pass the string "sugarss" to the `options.postcssOptions.parser` property of the loader.
However this violates symmetry and would require workarounds listed below.
You can simply pass `require("sugarss")` instead of the bare string, then include the `sugarss`
package in the `data` (runtime dependencies) of your `webpack.config.js`.

#### It's a bug

This is the case when a package has a `require` statement in its runtime code for some package, but
it doesn't list that package in its `package.json`, or lists it only as a `devDependency`.

pnpm and Yarn PnP will hit the same bug. Conveniently, there's already a shared database used by
both projects to list these, along with the missing dependency edge:
https://github.com/yarnpkg/berry/blob/master/packages/yarnpkg-extensions/sources/index.ts

> We should use this database under Bazel as well. Follow
> https://github.com/aspect-build/rules_js/issues/1215.

The recommended fix for both pnpm and rules_js is to use
[pnpm.packageExtensions](https://pnpm.io/package_json#pnpmpackageextensions)
in your `package.json` to add the missing `dependencies` or `peerDependencies`.

Example,

https://github.com/aspect-build/rules_js/blob/a8c192eed0e553acb7000beee00c60d60a32ed82/package.json#L12

> Make sure you pnpm install after changing `package.json`, as rules_js only reads the
> `pnpm-lock.yaml` file to gather dependency information.
> See [Fetch third-party packages](./README.md#fetch-third-party-packages-from-npm)

#### It's a plugin

Sometimes the package intentionally doesn't list dependencies, because it discovers them at runtime.
This is used for tools that locate their "plugins"; `eslint` and `prettier` are common typical examples.

The solution is based on pnpm's [public-hoist-pattern](https://pnpm.io/npmrc#public-hoist-pattern).
Use the [`public_hoist_packages` attribute of `npm_translate_lock`](./npm_translate_lock.md#npm_translate_lock-public_hoist_packages).
The documentation says the value provided to each element in the map is:

> a list of Bazel packages in which to hoist the package to the top-level of the node_modules tree

To make plugins work, you should have the Bazel package containing the pnpm workspace root (the folder containing `pnpm-lock.yaml`) in this list.
This ensures that the tool in the pnpm virtual store `node_modules/.aspect_rules_js` will be able to locate the plugins.
If your lockfile is in the root of the Bazel workspace, this value should be an empty string: `""`.
If the lockfile is in `some/subpkg/pnpm-lock.yaml` then `"some/subpkg"` should appear in the list.

For example:

`WORKSPACE`

```starlark
npm_translate_lock(
    ...
    public_hoist_packages = {
        "eslint-config-react-app": [""],
    },
)
```

Note that `public_hoist_packages` affects the layout of the `node_modules` tree, but you still need
to depend on that hoisted package, e.g. with `deps = [":node_modules/hoisted_pkg"]`. Continuing the example:

`BUILD`

```starlark
eslint_bin.eslint_test(
    ...
    data = [
        ...
        "//:node_modules/eslint-config-react-app",
    ],
)
```

> NB: We plan to add support for the `.npmrc` `public-hoist-pattern` setting to `rules_js` in a future release.
> For now, you must emulate public-hoist-pattern in `rules_js` using the `public_hoist_packages` attribute shown above.

## Performance

For general bazel performance tips see the [Aspect bazelrc guide](https://docs.aspect.build/guides/bazelrc/#performance-options).

### Parallelism (build, test)

A lot of tooling in the JS ecosystem uses parallelism to speed up builds. This is great, but as Bazel also parallels builds this can lead to a lot of contention for resources.

Some rulesets configure tools to take this into account such as the [rules_jest](https://github.com/aspect-build/rules_jest) default [run_in_band](https://github.com/aspect-build/rules_jest/blob/main/docs/jest_test.md#jest_test-run_in_band), while other tools (especially those without dedicated rulesets) may need to be configured manually.

For example, the [default WebPack configuration](https://webpack.js.org/configuration/optimization/#optimizationminimizer) uses Terser for optimization. `terser-webpack-plugin` defaults to [parallelizing its work across os.cpus().length - 1](https://www.npmjs.com/package/terser-webpack-plugin#parallel).
This can lead to builds performing slower due to IO throttling, or even failing if running in a virtualized environment where IO throughput is limited.

If you are experiencing slower than expected builds, you can try disabling or reducing parallelism for the tools you are using.

#### Jest

See [rules_jest](https://github.com/aspect-build/rules_jest) specific [troubleshooting](https://docs.aspect.build/rulesets/aspect_rules_jest/docs/troubleshooting#performance).
