const fs = require('fs')
const exists = require('path-exists')
const os = require('os')
const path = require('path')
const { safeReadPackageJsonFromDir } = require('@pnpm/read-package-json')
const { runLifecycleHook } = require('@pnpm/lifecycle')

async function mkdirp(p) {
    if (p && !fs.existsSync(p)) {
        await mkdirp(path.dirname(p))
        await fs.promises.mkdir(p)
    }
}

function normalizeBinPath(p) {
    let result = p.replace(/\\/g, '/')
    if (result.startsWith('./')) {
        result = result.substring(2)
    }
    return result
}

async function makeBins(nodeModulesPath, scope, segmentsUp) {
    const packages = await fs.promises.readdir(
        path.join(nodeModulesPath, scope)
    )
    for (const _package of packages) {
        if (!scope && _package.startsWith('@')) {
            await makeBins(nodeModulesPath, _package, segmentsUp)
            continue
        }
        const packageName = path.join(scope, _package)
        const packageJsonPath = path.join(
            nodeModulesPath,
            packageName,
            'package.json'
        )
        if (fs.existsSync(packageJsonPath)) {
            let packageJsonStr = await fs.promises.readFile(packageJsonPath)
            let packageJson
            try {
                packageJson = JSON.parse(packageJsonStr)
            } catch (e) {
                // Catch and throw a more detailed error message.
                throw new Error(
                    `Error parsing ${packageName}/package.json: ${e}\n\n""""\n${packageJsonStr}\n""""`
                )
            }

            // https://docs.npmjs.com/cli/v7/configuring-npm/package-json#bin
            if (packageJson.bin) {
                await mkdirp(path.join(nodeModulesPath, '.bin'))
                let bin = packageJson.bin
                if (typeof bin == 'string') {
                    bin = { [_package]: bin }
                }
                for (const binName of Object.keys(bin)) {
                    if (binName.includes('/') || binName.includes('\\')) {
                        // multi-segment bin names are not supported; pnpm itself
                        // also does not make .bin entries in this case as of pnpm v8.3.1
                        continue
                    }
                    const binPath = normalizeBinPath(bin[binName])
                    let binEntryPath = path.join(
                        nodeModulesPath,
                        '.bin',
                        binName
                    )
                    let binExec
                    if (isWindows()) {
                        binEntryPath += '.cmd'
                        binExec = `node "${path.join(
                            ...segmentsUp,
                            packageName,
                            binPath
                        )}" "%*"`
                    } else {
                        binExec = `#!/usr/bin/env bash\nexec node "${path.join(
                            ...segmentsUp,
                            packageName,
                            binPath
                        )}" "$@"`
                    }
                    await fs.promises.writeFile(binEntryPath, binExec)
                    await fs.promises.chmod(binEntryPath, '755') // executable
                }
            }
        }
    }
}

// Helper which is exported from @pnpm/lifecycle:
// https://github.com/pnpm/pnpm/blob/bc18d33fe00d9ed43f1562d4cc6d37f49d9c2c38/exec/lifecycle/src/index.ts#L52
async function checkBindingGyp(root, scripts) {
    if (await exists(path.join(root, 'binding.gyp'))) {
        scripts['install'] = 'node-gyp rebuild'
    }
}

// Like runPostinstallHooks from @pnpm/lifecycle at
// https://github.com/pnpm/pnpm/blob/bc18d33fe00d9ed43f1562d4cc6d37f49d9c2c38/exec/lifecycle/src/index.ts#L20
// but also runs a customizable list of lifecycle hooks.
async function runLifecycleHooks(opts, hooks) {
    const pkg = await safeReadPackageJsonFromDir(opts.pkgRoot)
    if (pkg == null) {
        return
    }
    if (pkg.scripts == null) {
        pkg.scripts = {}
    }

    const runInstallScripts =
        hooks.includes('preinstall') ||
        hooks.includes('install') ||
        hooks.includes('postinstall')
    if (runInstallScripts && !pkg.scripts.install) {
        await checkBindingGyp(opts.pkgRoot, pkg.scripts)
    }

    for (const hook of hooks) {
        if (pkg.scripts[hook]) {
            await runLifecycleHook(hook, pkg, opts)
        }
    }
}

function isWindows() {
    return os.platform() === 'win32'
}

async function main(args) {
    if (args.length < 3) {
        console.error(
            'Usage: node lifecycle-hooks.js [packageName] [packageDir] [outputDir] [--arch=...]? [--platform=...]?'
        )
        process.exit(1)
    }
    const packageName = args[0]
    const packageDir = args[1]
    const outputDir = args[2]

    let platform = null
    let arch = null
    let libc = null
    // This is naive "parsing" of the argv, but allows to avoid bringing in additional dependencies:
    for (let i = 3; i < args.length; ++i) {
        let found = args[i].match(/--arch=(.*)/)
        if (found) {
            arch = found[1]
        }
        found = args[i].match(/--platform=(.*)/)
        if (found) {
            platform = found[1]
        }
        found = args[i].match(/--libc=(.*)/)
        if (found) {
            libc = found[1]
        }
    }

    await copyPackageContents(packageDir, outputDir)

    // Resolve the path to the node_modules folder for this package in the symlinked node_modules
    // tree. Output path is of the format,
    //    .../node_modules/.aspect_rules_js/package@version/node_modules/package
    //    .../node_modules/.aspect_rules_js/@scope+package@version/node_modules/@scope/package
    // Path to node_modules is one or two segments up from the output path depending on the packageName
    const segmentsUp = Array(packageName.split('/').length).fill('..')
    const nodeModulesPath = path.resolve(path.join(outputDir, ...segmentsUp))

    // Create .bin entry point files for all packages in node_modules
    await makeBins(nodeModulesPath, '', segmentsUp)
    // export interface RunLifecycleHookOptions {
    //     args?: string[];
    //     depPath: string;
    //     extraBinPaths?: string[];
    //     extraEnv?: Record<string, string>;
    //     initCwd?: string;
    //     optional?: boolean;
    //     pkgRoot: string;
    //     rawConfig: object;
    //     rootModulesDir: string;
    //     scriptShell?: string;
    //     silent?: boolean;
    //     scriptsPrependNodePath?: boolean | 'warn-only';
    //     shellEmulator?: boolean;
    //     stdio?: string;
    //     unsafePerm: boolean;
    // }

    // We need to explicitly pass `npm_config_` prefixed env-variables as configuration to the lifecycle hook (or gyp).
    // 1. rules_js allow to provide per action_type environment using `lifecycle_hooks_envs`.
    // 2. One of the important use-cases is to able provide mirror where prebuild binaries are stored:
    //    (see: https://github.com/mapbox/node-pre-gyp#download-binary-files-from-a-mirror
    //    by {module_name}_binary_host_mirror)
    // 3. Such flags are taken (by gyp) from environment variables:
    //    https://github.com/mapbox/node-pre-gyp/blob/a74f5e367c0d71033620aa0112e7baf7f3515b9d/lib/util/versioning.js#L316
    // 4. Unfortunetely pnpm/lifecycle drops all npm_ prefixed env-variables prior to calling lifecycle hook:
    //      https://github.com/pnpm/npm-lifecycle/blob/99ac0429025bdf1303879723d3fbd57c585ae8a1/index.js#L351
    //    and later recreates it based on explicitly given config:
    //      https://github.com/pnpm/npm-lifecycle/blob/99ac0429025bdf1303879723d3fbd57c585ae8a1/index.js#L408
    // 5. So we need to perform reversed process: generate rawConfig based on env-variables to preserve them.
    let inherited_env = {}
    const npm_config_prefix = 'npm_config_'
    const config_regexp = new RegExp('^' + npm_config_prefix, 'i')
    for (let e in process.env) {
        if (e.match(config_regexp)) {
            inherited_env[e.substring(npm_config_prefix.length)] =
                process.env[e]
        }
    }

    const opts = {
        pkgRoot: path.resolve(outputDir),

        // rawConfig is passed as `config {...}`
        //   in @pnpm/lifecycle: https://github.com/pnpm/pnpm/blob/0da8703063797f59b01523f4283b9bd27123d063/exec/lifecycle/src/runLifecycleHook.ts#L65
        // echo property within `config {...}` is exposed in env_variable 'npm_config_'
        //   by @pnpm/npm-lifecycle: https://github.com/pnpm/npm-lifecycle/blob/99ac0429025bdf1303879723d3fbd57c585ae8a1/index.js#L434
        // The lifecycle hooks can interpret the npm_config_arch and npm_config_platform env variables:
        //   e.g. sharp: https://github.com/lovell/sharp/blob/9c217ab580123ee14ad65d5043d74d8ea7c245e5/lib/platform.js#L12
        // The npm_config_arch & npm_config_platform conversion is obeyed by tools like prebuild-install:
        //     https://yarnpkg.com/package?name=prebuild-install
        //     ("... you can set environment variables npm_config_build_from_source=true, npm_config_platform"
        //      npm_config_arch, npm_config_target npm_config_runtime and npm_config_libc").
        // or node-pre-gyp:
        //     https://github.com/mapbox/node-pre-gyp/blob/a74f5e367c0d71033620aa0112e7baf7f3515b9d/lib/node-pre-gyp.js#L188
        //
        rawConfig: Object.assign(
            {},
            {
                stdio: 'inherit',
                platform: platform,
                target_platform: platform, // Interpreted by https://github.com/mapbox/node-pre-gyp
                arch: arch,
                target_arch: arch, // node-pre-gyp
                libc: libc,
                target_libc: libc, // node-pre-gyp
            },
            inherited_env
        ),
        silent: false,
        stdio: 'inherit',
        rootModulesDir: nodeModulesPath,
        unsafePerm: true, // Don't run under a specific user/group
    }

    const rulesJsJson = JSON.parse(
        await fs.promises.readFile(
            path.join(packageDir, 'aspect_rules_js_metadata.json')
        )
    )

    if (rulesJsJson.lifecycle_hooks) {
        // Runs configured lifecycle hooks
        await runLifecycleHooks(opts, rulesJsJson.lifecycle_hooks.split(','))
    }

    if (rulesJsJson.scripts?.custom_postinstall) {
        // Run user specified custom postinstall hook
        await runLifecycleHook('custom_postinstall', rulesJsJson, opts)
    }
}

// Copy contents of a package dir to a destination dir (without copying the package dir itself)
async function copyPackageContents(packageDir, destDir) {
    const contents = await fs.promises.readdir(packageDir)
    await Promise.all(
        contents.map((file) =>
            copyRecursive(path.join(packageDir, file), path.join(destDir, file))
        )
    )
}

// Recursively copy files and folders
async function copyRecursive(src, dest) {
    const stats = await fs.promises.stat(src)
    if (stats.isDirectory()) {
        await mkdirp(dest)
        const contents = await fs.promises.readdir(src)
        await Promise.all(
            contents.map((file) =>
                copyRecursive(path.join(src, file), path.join(dest, file))
            )
        )
    } else {
        await fs.promises.copyFile(src, dest)
    }
}

;(async () => {
    try {
        await main(process.argv.slice(2))
    } catch (e) {
        // Note: use .log rather than .error. The former is deferred and the latter is immediate.
        // The error is harder to spot and parse when it appears in the middle of the other logs.
        if (e.code === 'ELIFECYCLE' && !!e.pkgid && !!e.stage && !!e.script) {
            console.log(
                '==============================================================='
            )
            console.log(
                `Failure while running lifecycle hook for package '${e.pkgid}':\n`
            )
            console.log(`  Script:  '${e.stage}'`)
            console.log(`  Command: \`${e.script}\``)
            console.log(`\nStack trace:\n`)
            // First line of error is always the message, which is redundant with the above logging.
            console.log(e.stack.replace(/^.*?\n/, ''))
            console.log(
                '==============================================================='
            )
        } else {
            console.log(e)
        }

        process.exit(1)
    }
})()
