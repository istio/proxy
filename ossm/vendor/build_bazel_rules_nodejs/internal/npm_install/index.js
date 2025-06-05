/* THIS FILE GENERATED FROM .ts; see BUILD.bazel */ /* clang-format off */'use strict';
Object.defineProperty(exports, "__esModule", { value: true });
exports.printIndexBzl = exports.printPackageBin = exports.parsePackage = exports.getDirectDependencySet = exports.main = void 0;
const fs_1 = require("fs");
const path = require("path");
const crypto = require("crypto");
function log_verbose(...m) {
    if (!!process.env['VERBOSE_LOGS'])
        console.error('[generate_build_file.ts]', ...m);
}
const PUBLIC_VISIBILITY = '//visibility:public';
let LEGACY_NODE_MODULES_PACKAGE_NAME = '$node_modules$';
let config = {
    exports_directories_only: false,
    generate_local_modules_build_files: false,
    generate_build_files_concurrency_limit: 64,
    included_files: [],
    links: {},
    package_json: 'package.json',
    package_lock: 'yarn.lock',
    package_path: '',
    rule_type: 'yarn_install',
    strict_visibility: true,
    workspace_rerooted_path: '',
    workspace: '',
    all_node_modules_target_name: 'node_modules',
};
function generateBuildFileHeader(visibility = PUBLIC_VISIBILITY) {
    return `# Generated file from ${config.rule_type} rule.
# See rules_nodejs/internal/npm_install/generate_build_file.ts

package(default_visibility = ["${visibility}"])

`;
}
if (require.main === module) {
    main();
}
const compareDep = (a, b) => {
    if (a._dir < b._dir)
        return -1;
    if (a._dir > b._dir)
        return 1;
    return 0;
};
const constant = (c) => () => c;
async function exists(p) {
    return fs_1.promises.access(p, fs_1.constants.F_OK | fs_1.constants.W_OK)
        .then(constant(true), constant(false));
}
const mkdirPromiseMap = new Map();
async function mkdirp(p) {
    let promise = mkdirPromiseMap.get(p);
    if (!promise) {
        promise = (async () => {
            if (await exists(p))
                return;
            await mkdirp(path.dirname(p));
            await fs_1.promises.mkdir(p);
        })();
        mkdirPromiseMap.set(p, promise);
    }
    await promise;
}
async function writeFile(p, content) {
    await mkdirp(path.dirname(p));
    await fs_1.promises.writeFile(p, content);
}
async function createFileSymlink(target, p) {
    await mkdirp(path.dirname(p));
    await fs_1.promises.symlink(target, p, 'file');
}
async function main() {
    config = require('./generate_config.json');
    config.limited_visibility = `@${config.workspace}//:__subpackages__`;
    const deps = await getDirectDependencySet(config.package_json);
    const pkgs = [];
    await findPackagesAndPush(pkgs, nodeModulesFolder(), deps);
    pkgs.sort(compareDep);
    flattenDependencies(pkgs);
    await generateBazelWorkspaces(pkgs);
    await generateBuildFiles(pkgs);
    await writeFile('.bazelignore', `node_modules\n${config.workspace_rerooted_path}`);
}
exports.main = main;
function nodeModulesFolder() {
    return config.exports_directories_only ?
        `${config.workspace_rerooted_package_json_dir}/node_modules` :
        'node_modules';
}
async function generateBuildFiles(pkgs) {
    const notNestedPkgs = pkgs.filter(pkg => !pkg._isNested);
    await generateRootBuildFile(notNestedPkgs);
    await notNestedPkgs.reduce((p, pkg) => p.then(() => generatePackageBuildFiles(pkg)), Promise.resolve());
    await (await findScopes()).reduce((prev, scope) => prev.then(() => generateScopeBuildFiles(scope, pkgs)), Promise.resolve());
    await generateLinksBuildFiles(config.links);
}
async function generateLinksBuildFiles(links) {
    for (const packageName of Object.keys(links)) {
        const target = links[packageName];
        const basename = packageName.split('/').pop();
        const starlark = generateBuildFileHeader() +
            `load("@build_bazel_rules_nodejs//internal/linker:npm_link.bzl", "npm_link")
npm_link(
    name = "${basename}",
    target = "${target}",
    package_name = "${packageName}",
    package_path = "${config.package_path}",
)`;
        await writeFile(path.posix.join(packageName, 'BUILD.bazel'), starlark);
    }
}
function flattenDependencies(pkgs) {
    const pkgsMap = new Map();
    pkgs.forEach(pkg => pkgsMap.set(pkg._dir, pkg));
    pkgs.forEach(pkg => flattenPkgDependencies(pkg, pkg, pkgsMap));
}
async function generateRootBuildFile(pkgs) {
    let buildFile = config.exports_directories_only ?
        printRootExportsDirectories(pkgs) :
        printRootExportsAllFiles(pkgs);
    try {
        const manualContents = await fs_1.promises.readFile(`manual_build_file_contents`, { encoding: 'utf8' });
        buildFile += '\n\n';
        buildFile += manualContents;
    }
    catch (e) {
    }
    await writeFile('BUILD.bazel', buildFile);
}
function printRootExportsDirectories(pkgs) {
    let filegroupsStarlark = '';
    pkgs.forEach(pkg => {
        filegroupsStarlark += `
copy_file(
  name = "node_modules/${pkg._dir}",
  src = "${config.workspace_rerooted_package_json_dir}/node_modules/${pkg._dir}",
  is_directory = True,
  out = "node_modules/${pkg._dir}",
  visibility = ["//visibility:public"],
)
js_library(
    name = "${pkg._dir.replace("/", "_")}__contents",
    package_name = "${pkg._dir}",
    package_path = "${config.package_path}",
    strip_prefix = "node_modules/${pkg._dir}",
    srcs = [":node_modules/${pkg._dir}"],
    visibility = ["//:__subpackages__"],
)
`;
    });
    let depsStarlark = '';
    if (pkgs.length) {
        const list = pkgs.map(pkg => `"//${pkg._dir}:${pkg._name}",`).join('\n        ');
        depsStarlark = `
  deps = [
      ${list}
  ],`;
    }
    const result = generateBuildFileHeader() + `load("@build_bazel_rules_nodejs//:index.bzl", "js_library")
load("@build_bazel_rules_nodejs//third_party/github.com/bazelbuild/bazel-skylib:rules/copy_file.bzl", "copy_file")

# To support remote-execution, we must create a tree artifacts from the source directories.
# We make the output node_modules/pkg_dir so that we get a free node_modules
# tree in bazel-out and runfiles for this external repository.
${filegroupsStarlark}

# The node_modules directory in one catch-all js_library
js_library(
  name = "node_modules",${depsStarlark}
)`;
    return result;
}
function printRootExportsAllFiles(pkgs) {
    let pkgFilesStarlark = '';
    if (pkgs.length) {
        let list = '';
        list = pkgs.map(pkg => `"//${pkg._dir}:${pkg._name}__files",`).join('\n        ');
        list += '\n        ';
        list += pkgs.map(pkg => `"//${pkg._dir}:${pkg._name}__nested_node_modules",`).join('\n        ');
        pkgFilesStarlark = `
    # direct sources listed for strict deps support
    srcs = [
        ${list}
    ],`;
    }
    let depsStarlark = '';
    if (pkgs.length) {
        const list = pkgs.map(pkg => `"//${pkg._dir}:${pkg._name}__contents",`).join('\n        ');
        depsStarlark = `
    # flattened list of direct and transitive dependencies hoisted to root by the package manager
    deps = [
        ${list}
    ],`;
    }
    let exportsStarlark = '';
    pkgs.forEach(pkg => {
        pkg._files.forEach(f => {
            exportsStarlark += `    "node_modules/${pkg._dir}/${f}",\n`;
        });
    });
    const result = generateBuildFileHeader() + `load("@build_bazel_rules_nodejs//:index.bzl", "js_library")

exports_files([
${exportsStarlark}])

# The node_modules directory in one catch-all js_library.
# NB: Using this target may have bad performance implications if
# there are many files in target.
# See https://github.com/bazelbuild/bazel/issues/5153.
js_library(
    name = "${config.all_node_modules_target_name}",
    package_name = "${LEGACY_NODE_MODULES_PACKAGE_NAME}",
    package_path = "${config.package_path}",${pkgFilesStarlark}${depsStarlark}
)

`;
    return result;
}
async function generatePackageBuildFiles(pkg) {
    let buildFilePath;
    if (pkg._files.includes('BUILD'))
        buildFilePath = 'BUILD';
    if (pkg._files.includes('BUILD.bazel'))
        buildFilePath = 'BUILD.bazel';
    const nodeModules = nodeModulesFolder();
    const nodeModulesPkgDir = `${nodeModules}/${pkg._dir}`;
    const isPkgDirASymlink = await fs_1.promises.lstat(nodeModulesPkgDir)
        .then(stat => stat.isSymbolicLink())
        .catch(constant(false));
    const symlinkBuildFile = isPkgDirASymlink && buildFilePath && !config.generate_local_modules_build_files;
    if (isPkgDirASymlink && !buildFilePath && !config.generate_local_modules_build_files) {
        console.log(`[yarn_install/npm_install]: package ${nodeModulesPkgDir} is local symlink and as such a BUILD file for it is expected but none was found. Please add one at ${await fs_1.promises.realpath(nodeModulesPkgDir)}`);
    }
    let buildFile = config.exports_directories_only ?
        printPackageExportsDirectories(pkg) :
        printPackageLegacy(pkg);
    if (buildFilePath) {
        buildFile = buildFile + '\n' +
            await fs_1.promises.readFile(path.join(nodeModules, pkg._dir, buildFilePath), 'utf-8');
    }
    else {
        buildFilePath = 'BUILD.bazel';
    }
    const visibility = !pkg._directDependency && config.strict_visibility ?
        config.limited_visibility :
        PUBLIC_VISIBILITY;
    if (!pkg._files.includes('bin/BUILD.bazel') && !pkg._files.includes('bin/BUILD')) {
        const binBuildFile = printPackageBin(pkg);
        if (binBuildFile.length) {
            await writeFile(path.posix.join(pkg._dir, 'bin', 'BUILD.bazel'), generateBuildFileHeader(visibility) + binBuildFile);
        }
    }
    const hasIndexBzl = pkg._files.includes('index.bzl');
    if (hasIndexBzl) {
        await pkg._files.filter(f => f !== 'BUILD' && f !== 'BUILD.bazel').reduce(async (prev, file) => {
            if (/^node_modules[/\\]/.test(file)) {
                return;
            }
            let destFile = path.posix.join(pkg._dir, file);
            const basename = path.basename(file);
            const basenameUc = basename.toUpperCase();
            if (basenameUc === '_BUILD' || basenameUc === '_BUILD.BAZEL') {
                destFile = path.posix.join(path.dirname(destFile), basename.substr(1));
            }
            const src = path.posix.join(nodeModules, pkg._dir, file);
            await prev;
            await mkdirp(path.dirname(destFile));
            await fs_1.promises.copyFile(src, destFile);
        }, Promise.resolve());
    }
    const indexFile = printIndexBzl(pkg);
    if (indexFile.length) {
        await writeFile(path.posix.join(pkg._dir, hasIndexBzl ? 'private' : '', 'index.bzl'), indexFile);
        const buildContent = `
# For integration testing
exports_files(["index.bzl"])
`;
        if (hasIndexBzl) {
            await writeFile(path.posix.join(pkg._dir, 'private', 'BUILD.bazel'), buildContent);
        }
        else {
            buildFile += buildContent;
        }
    }
    if (!symlinkBuildFile) {
        await writeFile(path.posix.join(pkg._dir, buildFilePath), generateBuildFileHeader(visibility) + buildFile);
    }
    else {
        const realPathBuildFileForPkg = await fs_1.promises.realpath(path.posix.join(nodeModulesPkgDir, buildFilePath));
        await createFileSymlink(realPathBuildFileForPkg, path.posix.join(pkg._dir, buildFilePath));
    }
}
async function generateBazelWorkspaces(pkgs) {
    const workspaces = {};
    for (const pkg of pkgs) {
        if (!pkg.bazelWorkspaces) {
            continue;
        }
        for (const workspace of Object.keys(pkg.bazelWorkspaces)) {
            if (workspaces[workspace]) {
                console.error(`Could not setup Bazel workspace ${workspace} requested by npm ` +
                    `package ${pkg._dir}@${pkg.version}. Already setup by ${workspaces[workspace]}`);
                process.exit(1);
            }
            await generateBazelWorkspace(pkg, workspace);
            workspaces[workspace] = `${pkg._dir}@${pkg.version}`;
        }
    }
}
async function generateBazelWorkspace(pkg, workspace) {
    let bzlFile = `# Generated by the yarn_install/npm_install rule
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")
load("@build_bazel_rules_nodejs//internal/copy_repository:copy_repository.bzl", "copy_repository")

`;
    const rootPath = pkg.bazelWorkspaces[workspace].rootPath;
    if (!rootPath) {
        console.error(`Malformed bazelWorkspaces attribute in ${pkg._dir}@${pkg.version}. ` +
            `Missing rootPath for workspace ${workspace}.`);
        process.exit(1);
    }
    const workspaceSourcePath = path.posix.join('_workspaces', workspace);
    const nodeModules = nodeModulesFolder();
    await mkdirp(workspaceSourcePath);
    await Promise.all(pkg._files.map(async (file) => {
        if (/^node_modules[/\\]/.test(file)) {
            return;
        }
        let destFile = path.relative(rootPath, file);
        if (destFile.startsWith('..')) {
            return;
        }
        const basename = path.basename(file);
        const basenameUc = basename.toUpperCase();
        if (basenameUc === '_BUILD' || basenameUc === '_BUILD.BAZEL') {
            destFile = path.posix.join(path.dirname(destFile), basename.substr(1));
        }
        const src = path.posix.join(nodeModules, pkg._dir, file);
        const dest = path.posix.join(workspaceSourcePath, destFile);
        await mkdirp(path.dirname(dest));
        await fs_1.promises.copyFile(src, dest);
    }));
    if (!hasRootBuildFile(pkg, rootPath)) {
        await writeFile(path.posix.join(workspaceSourcePath, 'BUILD.bazel'), '# Marker file that this directory is a bazel package');
    }
    const sha256sum = crypto.createHash('sha256');
    sha256sum.update(await fs_1.promises.readFile(config.package_lock, { encoding: 'utf8' }));
    await writeFile(path.posix.join(workspaceSourcePath, '_bazel_workspace_marker'), `# Marker file to used by custom copy_repository rule\n${sha256sum.digest('hex')}`);
    bzlFile += `def install_${workspace}():
    maybe(
        copy_repository,
        name = "${workspace}",
        marker_file = "@${config.workspace}//_workspaces/${workspace}:_bazel_workspace_marker",
    )
`;
    await writeFile(`install_${workspace}.bzl`, bzlFile);
}
async function generateScopeBuildFiles(scope, pkgs) {
    pkgs = pkgs.filter(pkg => !pkg._isNested && pkg._dir.startsWith(`${scope}/`));
    let deps = [];
    pkgs.forEach(pkg => {
        deps = deps.concat(pkg._dependencies.filter(dep => !dep._isNested && !pkgs.includes(pkg)));
    });
    deps = [...pkgs, ...new Set(deps)];
    let buildFile = config.exports_directories_only ?
        printScopeExportsDirectories(scope, deps) :
        printScopeLegacy(scope, deps);
    await writeFile(path.posix.join(scope, 'BUILD.bazel'), buildFile);
}
async function isFile(p) {
    return fs_1.promises.stat(p)
        .then(stat => stat.isFile())
        .catch(constant(false));
}
async function isDirectory(p) {
    return fs_1.promises.stat(p)
        .then((stat) => stat.isDirectory())
        .catch(constant(false));
}
function stripBom(s) {
    return s.charCodeAt(0) === 0xFEFF ? s.slice(1) : s;
}
async function listFilesAndPush(files, rootDir, subDir = '') {
    const dir = path.posix.join(rootDir, subDir);
    if (!isDirectory(dir)) {
        return;
    }
    const filelist = await fs_1.promises.readdir(dir);
    for (const file of filelist) {
        const fullPath = path.posix.join(dir, file);
        const relPath = path.posix.join(subDir, file);
        const isSymbolicLink = (await fs_1.promises.lstat(fullPath)).isSymbolicLink();
        let stat;
        try {
            stat = await fs_1.promises.stat(fullPath);
        }
        catch (e) {
            if (isSymbolicLink) {
                if (config.exports_directories_only) {
                    await fs_1.promises.unlink(fullPath);
                }
                continue;
            }
            throw e;
        }
        if (isSymbolicLink && config.exports_directories_only && path.basename(path.dirname(fullPath)) == '.bin') {
            await fs_1.promises.unlink(fullPath);
            continue;
        }
        const isDirectory = stat.isDirectory();
        if (isDirectory && isSymbolicLink) {
            if (config.exports_directories_only) {
                await fs_1.promises.unlink(fullPath);
            }
            continue;
        }
        if (isDirectory)
            await listFilesAndPush(files, rootDir, relPath);
        else
            files.push(relPath);
    }
}
function hasRootBuildFile(pkg, rootPath) {
    for (const file of pkg._files) {
        const fileUc = path.relative(rootPath, file).toUpperCase();
        if (fileUc === 'BUILD' || fileUc === 'BUILD.BAZEL' ||
            fileUc === '_BUILD' || fileUc === '_BUILD.BAZEL') {
            return true;
        }
    }
    return false;
}
async function getDirectDependencySet(pkgJsonPath) {
    const pkgJson = JSON.parse(stripBom(await fs_1.promises.readFile(pkgJsonPath, { encoding: 'utf8' })));
    return new Set([
        ...Object.keys(pkgJson.dependencies || {}),
        ...Object.keys(pkgJson.devDependencies || {}),
        ...Object.keys(pkgJson.optionalDependencies || {}),
    ]);
}
exports.getDirectDependencySet = getDirectDependencySet;
async function findPackagesAndPush(pkgs, p, dependencies) {
    if (!await isDirectory(p)) {
        return;
    }
    const listing = await fs_1.promises.readdir(p);
    let promises = listing.map(async (f) => {
        if (f.startsWith('.'))
            return [];
        const pf = path.posix.join(p, f);
        if (await isDirectory(pf)) {
            if (f.startsWith('@')) {
                await findPackagesAndPush(pkgs, pf, dependencies);
            }
            else {
                pkgs.push(await parsePackage(pf, dependencies));
                await findPackagesAndPush(pkgs, path.posix.join(pf, 'node_modules'), dependencies);
            }
        }
    });
    if (config.generate_build_files_concurrency_limit < 1) {
        await Promise.all(promises);
    }
    else {
        while (promises.length) {
            await Promise.all(promises.splice(0, config.generate_build_files_concurrency_limit));
        }
    }
}
async function findScopes() {
    const p = nodeModulesFolder();
    if (!await isDirectory(p)) {
        return [];
    }
    const listing = await fs_1.promises.readdir(p);
    const scopes = (await Promise.all(listing.map(async (f) => {
        if (!f.startsWith('@'))
            return;
        f = path.posix.join(p, f);
        if (await isDirectory(f)) {
            return f.substring(p.length + 1);
        }
    })))
        .filter((f) => typeof f === 'string');
    return scopes;
}
async function parsePackage(p, dependencies = new Set()) {
    const packageJson = path.posix.join(p, 'package.json');
    const pkg = (await isFile(packageJson)) ?
        JSON.parse(stripBom(await fs_1.promises.readFile(packageJson, { encoding: 'utf8' }))) :
        { version: '0.0.0' };
    pkg._dir = p.substring(nodeModulesFolder().length + 1);
    pkg._name = pkg._dir.split('/').pop();
    pkg._moduleName = pkg.name || `${pkg._dir}/${pkg._name}`;
    pkg._isNested = /\/node_modules\//.test(pkg._dir);
    pkg._files = [];
    await listFilesAndPush(pkg._files, p);
    pkg._files.sort();
    pkg._runfiles = pkg._files.filter((f) => !/[^\x21-\x7E]/.test(f));
    pkg._dependencies = [];
    pkg._directDependency = dependencies.has(pkg._moduleName) || dependencies.has(pkg._name) || dependencies.has(pkg._dir);
    return pkg;
}
exports.parsePackage = parsePackage;
function isValidBinPath(entry) {
    return isValidBinPathStringValue(entry) || isValidBinPathObjectValues(entry);
}
function isValidBinPathStringValue(entry) {
    return typeof entry === 'string' && entry !== '';
}
function isValidBinPathObjectValues(entry) {
    return entry && typeof entry === 'object' &&
        Object['values'](entry).filter(_entry => isValidBinPath(_entry)).length > 0;
}
function cleanupBinPath(p) {
    p = p.replace(/\\/g, '/');
    if (p.indexOf('./') === 0) {
        p = p.slice(2);
    }
    return p;
}
function cleanupEntryPointPath(p) {
    p = p.replace(/\\/g, '/');
    if (p.indexOf('./') === 0) {
        p = p.slice(2);
    }
    if (p.endsWith('/')) {
        p += 'index.js';
    }
    return p;
}
function findEntryFile(pkg, path) {
    const cleanPath = cleanupEntryPointPath(path);
    const entryFile = findFile(pkg, cleanPath) || findFile(pkg, `${cleanPath}.js`);
    if (!entryFile) {
        log_verbose(`could not find entry point for the path ${cleanPath} given by npm package ${pkg._name}`);
    }
    return entryFile;
}
function resolveMainFile(pkg, mainFileName) {
    const mainEntryField = pkg[mainFileName];
    if (mainEntryField) {
        if (typeof mainEntryField === 'string') {
            return findEntryFile(pkg, mainEntryField);
        }
        else if (typeof mainEntryField === 'object' && mainFileName === 'browser') {
            const indexEntryPoint = mainEntryField['index.js'] || mainEntryField['./index.js'];
            if (indexEntryPoint) {
                return findEntryFile(pkg, indexEntryPoint);
            }
        }
    }
}
function resolvePkgMainFile(pkg) {
    const mainFileNames = ['browser', 'module', 'main'];
    for (const mainFile of mainFileNames) {
        const resolvedMainFile = resolveMainFile(pkg, mainFile);
        if (resolvedMainFile) {
            return resolvedMainFile;
        }
    }
    const maybeRootIndex = findEntryFile(pkg, 'index.js');
    if (maybeRootIndex) {
        return maybeRootIndex;
    }
    const maybeSelfNamedIndex = findEntryFile(pkg, `${pkg._name}.js`);
    if (maybeSelfNamedIndex) {
        return maybeSelfNamedIndex;
    }
    log_verbose(`could not find entry point for npm package ${pkg._name}`);
    return undefined;
}
function flattenPkgDependencies(pkg, dep, pkgsMap) {
    if (pkg._dependencies.indexOf(dep) !== -1) {
        return;
    }
    pkg._dependencies.push(dep);
    const findDeps = function (targetDeps, required, depType) {
        Object.keys(targetDeps || {})
            .map(targetDep => {
            const dirSegments = dep._dir.split('/');
            while (dirSegments.length) {
                const maybe = path.posix.join(...dirSegments, 'node_modules', targetDep);
                if (pkgsMap.has(maybe)) {
                    return pkgsMap.get(maybe);
                }
                dirSegments.pop();
            }
            if (pkgsMap.has(targetDep)) {
                return pkgsMap.get(targetDep);
            }
            if (required) {
                console.error(`could not find ${depType} '${targetDep}' of '${dep._dir}'`);
                process.exit(1);
            }
            return null;
        })
            .filter((dep) => Boolean(dep))
            .forEach(dep => flattenPkgDependencies(pkg, dep, pkgsMap));
    };
    if (dep.dependencies && dep.optionalDependencies) {
        Object.keys(dep.optionalDependencies).forEach(optionalDep => {
            delete dep.dependencies[optionalDep];
        });
    }
    findDeps(dep.dependencies, true, 'dependency');
    findDeps(dep.peerDependencies, false, 'peer dependency');
    findDeps(dep.optionalDependencies, false, 'optional dependency');
}
function printJson(pkg) {
    const cloned = { ...pkg };
    cloned._dependencies = pkg._dependencies.map(dep => dep._dir);
    delete cloned._files;
    delete cloned._runfiles;
    return JSON.stringify(cloned, null, 2).split('\n').map(line => `# ${line}`).join('\n');
}
function filterFiles(files, exts = []) {
    if (exts.length) {
        const allowNoExts = exts.includes('');
        files = files.filter(f => {
            if (allowNoExts && !path.extname(f))
                return true;
            const lc = f.toLowerCase();
            for (const e of exts) {
                if (e && lc.endsWith(e.toLowerCase())) {
                    return true;
                }
            }
            return false;
        });
    }
    return files.filter(file => {
        const basenameUc = path.basename(file).toUpperCase();
        if (basenameUc === 'BUILD' || basenameUc === 'BUILD.BAZEL') {
            return false;
        }
        return true;
    });
}
function isNgApfPackage(pkg) {
    const set = new Set(pkg._files);
    if (set.has('ANGULAR_PACKAGE')) {
        return true;
    }
    const metadataExt = /\.metadata\.json$/;
    return pkg._files.some((file) => {
        if (metadataExt.test(file)) {
            const sibling = file.replace(metadataExt, '.d.ts');
            if (set.has(sibling)) {
                return true;
            }
        }
        return false;
    });
}
function findFile(pkg, m) {
    const ml = m.toLowerCase();
    for (const f of pkg._files) {
        if (f.toLowerCase() === ml) {
            return f;
        }
    }
    return undefined;
}
function printPackageExportsDirectories(pkg) {
    const deps = [pkg].concat(pkg._dependencies.filter(dep => dep !== pkg && !dep._isNested));
    const depsStarlark = deps.map(dep => `"//:${dep._dir.replace("/", "_")}__contents",`).join('\n        ');
    return `load("@build_bazel_rules_nodejs//:index.bzl", "js_library")

# Generated targets for npm package "${pkg._dir}"
${printJson(pkg)}

# The primary target for this package for use in rule deps
js_library(
    name = "${pkg._name}",
    deps = [
        ${depsStarlark}
    ],
)
`;
}
function printPackageLegacy(pkg) {
    function starlarkFiles(attr, files, comment = '') {
        return `
    ${comment ? comment + '\n    ' : ''}${attr} = [
        ${files.map((f) => `"//:node_modules/${pkg._dir}/${f}",`).join('\n        ')}
    ],`;
    }
    const includedRunfiles = filterFiles(pkg._runfiles, config.included_files);
    const pkgFiles = includedRunfiles.filter((f) => !f.startsWith('node_modules/'));
    const pkgFilesStarlark = pkgFiles.length ? starlarkFiles('srcs', pkgFiles) : '';
    const nestedNodeModules = includedRunfiles.filter((f) => f.startsWith('node_modules/'));
    const nestedNodeModulesStarlark = nestedNodeModules.length ? starlarkFiles('srcs', nestedNodeModules) : '';
    const notPkgFiles = pkg._files.filter((f) => !f.startsWith('node_modules/') && !includedRunfiles.includes(f));
    const notPkgFilesStarlark = notPkgFiles.length ? starlarkFiles('srcs', notPkgFiles) : '';
    const namedSources = isNgApfPackage(pkg) ?
        filterFiles(pkg._runfiles, ['.umd.js', '.ngfactory.js', '.ngsummary.js']) :
        [];
    const namedSourcesStarlark = namedSources.length ?
        starlarkFiles('named_module_srcs', namedSources, '# subset of srcs that are javascript named-UMD or named-AMD scripts') :
        '';
    const dtsSources = filterFiles(pkg._runfiles, ['.d.ts']).filter((f) => !f.startsWith('node_modules/'));
    const dtsStarlark = dtsSources.length ?
        starlarkFiles('srcs', dtsSources, `# ${pkg._dir} package declaration files (and declaration files in nested node_modules)`) :
        '';
    const deps = [pkg].concat(pkg._dependencies.filter(dep => dep !== pkg && !dep._isNested));
    const depsStarlark = deps.map(dep => `"//${dep._dir}:${dep._name}__contents",`).join('\n        ');
    let result = `load("@build_bazel_rules_nodejs//:index.bzl", "js_library")

# Generated targets for npm package "${pkg._dir}"
${printJson(pkg)}

# Files that are part of the npm package not including its nested node_modules
# (filtered by the 'included_files' attribute)
filegroup(
    name = "${pkg._name}__files",${pkgFilesStarlark}
)

# Files that are in the npm package's nested node_modules
# (filtered by the 'included_files' attribute)
filegroup(
    name = "${pkg._name}__nested_node_modules",${nestedNodeModulesStarlark}
    visibility = ["//:__subpackages__"],
)

# Files that have been excluded from the ${pkg._name}__files target above because
# they are filtered out by 'included_files' or because they are not valid runfiles
# See https://github.com/bazelbuild/bazel/issues/4327.
filegroup(
    name = "${pkg._name}__not_files",${notPkgFilesStarlark}
    visibility = ["//visibility:private"],
)

# All of the files in the npm package including files that have been
# filtered out by 'included_files' or because they are not valid runfiles
# but not including nested node_modules.
filegroup(
    name = "${pkg._name}__all_files",
    srcs = [":${pkg._name}__files", ":${pkg._name}__not_files"],
)

# The primary target for this package for use in rule deps
js_library(
    name = "${pkg._name}",
    package_name = "${LEGACY_NODE_MODULES_PACKAGE_NAME}",
    package_path = "${config.package_path}",
    # direct sources listed for strict deps support
    srcs = [":${pkg._name}__files"],
    # nested node_modules for this package plus flattened list of direct and transitive dependencies
    # hoisted to root by the package manager
    deps = [
        ${depsStarlark}
    ],
)

# Target is used as dep for main targets to prevent circular dependencies errors
js_library(
    name = "${pkg._name}__contents",
    package_name = "${LEGACY_NODE_MODULES_PACKAGE_NAME}",
    package_path = "${config.package_path}",
    srcs = [":${pkg._name}__files", ":${pkg._name}__nested_node_modules"],${namedSourcesStarlark}
    visibility = ["//:__subpackages__"],
)

# Typings files that are part of the npm package not including nested node_modules
js_library(
    name = "${pkg._name}__typings",
    package_name = "${LEGACY_NODE_MODULES_PACKAGE_NAME}",
    package_path = "${config.package_path}",${dtsStarlark}
)

`;
    let mainEntryPoint = resolvePkgMainFile(pkg);
    if (mainEntryPoint && !findFile(pkg, `${pkg._name}.umd.js`)) {
        result +=
            `load("@build_bazel_rules_nodejs//internal/npm_install:npm_umd_bundle.bzl", "npm_umd_bundle")

npm_umd_bundle(
    name = "${pkg._name}__umd",
    package_name = "${pkg._moduleName}",
    entry_point = "@${config.workspace}//:node_modules/${pkg._dir}/${mainEntryPoint}",
    package = ":${pkg._name}",
)

`;
    }
    return result;
}
function _findExecutables(pkg) {
    const executables = new Map();
    if (isValidBinPath(pkg.bin)) {
        if (!pkg._isNested) {
            if (Array.isArray(pkg.bin)) {
                if (pkg.bin.length == 1) {
                    executables.set(pkg._dir, cleanupBinPath(pkg.bin[0]));
                }
                else {
                }
            }
            else if (typeof pkg.bin === 'string') {
                executables.set(pkg._dir, cleanupBinPath(pkg.bin));
            }
            else if (typeof pkg.bin === 'object') {
                for (let key in pkg.bin) {
                    if (isValidBinPathStringValue(pkg.bin[key])) {
                        executables.set(key, cleanupBinPath(pkg.bin[key]));
                    }
                }
            }
        }
    }
    return executables;
}
function additionalAttributes(pkg, name) {
    let additionalAttributes = '';
    if (pkg.bazelBin && pkg.bazelBin[name] && pkg.bazelBin[name].additionalAttributes) {
        const attrs = pkg.bazelBin[name].additionalAttributes;
        for (const attrName of Object.keys(attrs)) {
            const attrValue = attrs[attrName];
            additionalAttributes += `\n    ${attrName} = ${attrValue},`;
        }
    }
    return additionalAttributes;
}
function printPackageBin(pkg) {
    let result = '';
    const executables = _findExecutables(pkg);
    if (executables.size) {
        result = `load("@build_bazel_rules_nodejs//:index.bzl", "nodejs_binary")

`;
        const data = [`//${pkg._dir}:${pkg._name}`];
        if (pkg._dynamicDependencies) {
            data.push(...pkg._dynamicDependencies);
        }
        for (const [name, path] of executables.entries()) {
            const entryPoint = config.exports_directories_only ?
                `{ "@${config.workspace}//:node_modules/${pkg._dir}": "${path}" }` :
                `"@${config.workspace}//:node_modules/${pkg._dir}/${path}"`;
            result += `# Wire up the \`bin\` entry \`${name}\`
nodejs_binary(
    name = "${name}",
    entry_point = ${entryPoint},
    data = [${data.map(p => `"${p}"`).join(', ')}],${additionalAttributes(pkg, name)}
)
`;
        }
    }
    return result;
}
exports.printPackageBin = printPackageBin;
function printIndexBzl(pkg) {
    let result = '';
    const executables = _findExecutables(pkg);
    if (executables.size) {
        result =
            `load("@build_bazel_rules_nodejs//:index.bzl", "nodejs_binary", "nodejs_test", "npm_package_bin")

`;
        const data = [`@${config.workspace}//${pkg._dir}:${pkg._name}`];
        if (pkg._dynamicDependencies) {
            data.push(...pkg._dynamicDependencies);
        }
        for (const [name, path] of executables.entries()) {
            const entryPoint = config.exports_directories_only ?
                `{ "@${config.workspace}//:node_modules/${pkg._dir}": "${path}" }` :
                `"@${config.workspace}//:node_modules/${pkg._dir}/${path}"`;
            result = `${result}

# Generated helper macro to call ${name}
def ${name.replace(/-/g, '_')}(**kwargs):
    output_dir = kwargs.pop("output_dir", False)
    if "outs" in kwargs or output_dir:
        npm_package_bin(tool = "@${config.workspace}//${pkg._dir}/bin:${name}", output_dir = output_dir, **kwargs)
    else:
        nodejs_binary(
            entry_point = ${entryPoint},
            data = [${data.map(p => `"${p}"`).join(', ')}] + kwargs.pop("data", []),${additionalAttributes(pkg, name)}
            **kwargs
        )

# Just in case ${name} is a test runner, also make a test rule for it
def ${name.replace(/-/g, '_')}_test(**kwargs):
    nodejs_test(
      entry_point = ${entryPoint},
      data = [${data.map(p => `"${p}"`).join(', ')}] + kwargs.pop("data", []),${additionalAttributes(pkg, name)}
      **kwargs
    )
`;
        }
    }
    return result;
}
exports.printIndexBzl = printIndexBzl;
function printScopeLegacy(scope, deps) {
    let pkgFilesStarlark = '';
    if (deps.length) {
        const list = deps.map(dep => `"//${dep._dir}:${dep._name}__files",`).join('\n        ');
        pkgFilesStarlark = `
    # direct sources listed for strict deps support
    srcs = [
        ${list}
    ],`;
    }
    let depsStarlark = '';
    if (deps.length) {
        const list = deps.map(dep => `"//${dep._dir}:${dep._name}__contents",`).join('\n        ');
        depsStarlark = `
    # flattened list of direct and transitive dependencies hoisted to root by the package manager
    deps = [
        ${list}
    ],`;
    }
    return generateBuildFileHeader() + `load("@build_bazel_rules_nodejs//:index.bzl", "js_library")

# Generated target for npm scope ${scope}
js_library(
    name = "${scope}",
    package_name = "${LEGACY_NODE_MODULES_PACKAGE_NAME}",
    package_path = "${config.package_path}",${pkgFilesStarlark}${depsStarlark}
)

`;
}
function printScopeExportsDirectories(scope, deps) {
    let depsStarlark = '';
    if (deps.length) {
        const list = deps.map(dep => `"//${dep._dir}",`).join('\n        ');
        depsStarlark = `
    # flattened list of direct and transitive dependencies hoisted to root by the package manager
    deps = [
        ${list}
    ],`;
    }
    return generateBuildFileHeader() + `load("@build_bazel_rules_nodejs//:index.bzl", "js_library")

# Generated target for npm scope ${scope}
js_library(
    name = "${scope}",
    ${depsStarlark}
)

`;
}
