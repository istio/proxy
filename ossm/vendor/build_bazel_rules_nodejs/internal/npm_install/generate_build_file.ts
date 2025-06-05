/**
 * @license
 * Copyright 2017 The Bazel Authors. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/**
 * @fileoverview This script generates BUILD.bazel files by analyzing
 * the node_modules folder layed out by yarn or npm. It generates
 * fine grained Bazel `js_library` targets for each root npm package
 * and all files for that package and its transitive deps are included
 * in the target. For example, `@<workspace>//jasmine` would
 * include all files in the jasmine npm package and all of its
 * transitive dependencies.
 *
 * nodejs_binary targets are also generated for all `bin` scripts
 * in each package. For example, the `@<workspace>//jasmine/bin:jasmine`
 * target will be generated for the `jasmine` binary in the `jasmine`
 * npm package.
 *
 * Additionally, a `@<workspace>//:node_modules` `js_library`
 * is generated that includes all packages under node_modules
 * as well as the .bin folder.
 *
 * This work is based off the fine grained deps concepts in
 * https://github.com/pubref/rules_node developed by @pcj.
 *
 * @see https://docs.google.com/document/d/1AfjHMLVyE_vYwlHSK7k7yW_IIGppSxsQtPm9PTr1xEo
 */
'use strict';

import {promises as fs, constants, mkdir} from 'fs';
import * as path from 'path';
import * as crypto from 'crypto';

function log_verbose(...m: any[]) {
  if (!!process.env['VERBOSE_LOGS']) console.error('[generate_build_file.ts]', ...m);
}

const PUBLIC_VISIBILITY = '//visibility:public';

let LEGACY_NODE_MODULES_PACKAGE_NAME = '$node_modules$';

// Default values for unit testing; overridden in main()
let config: any = {
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

function generateBuildFileHeader(visibility = PUBLIC_VISIBILITY): string {
  return `# Generated file from ${config.rule_type} rule.
# See rules_nodejs/internal/npm_install/generate_build_file.ts

package(default_visibility = ["${visibility}"])

`;
}

if (require.main === module) {
  main();
}

const compareDep = (a: Dep, b: Dep) => {
  if(a._dir < b._dir) return -1;
  if(a._dir > b._dir) return 1; 
  return 0;
}

const constant = <T>(c: T) => () => c

async function exists(p: string) {
  return fs.access(p, constants.F_OK | constants.W_OK)
  .then(constant(true), constant(false));
}

// Avoid duplicate mkdir call when mkdir runs parallel.
const mkdirPromiseMap = new Map<string, Promise<void>>();

/**
 * Create a new directory and any necessary subdirectories
 * if they do not exist.
 */
async function mkdirp(p: string) {
  let promise = mkdirPromiseMap.get(p);
  if (!promise) {
    promise = (async () => {
      if (await exists(p)) return;
      await mkdirp(path.dirname(p));
      await fs.mkdir(p);
    })();
    // Store mkdir call
    mkdirPromiseMap.set(p, promise);
  }
  await promise;
}

/**
 * Writes a file, first ensuring that the directory to
 * write to exists.
 */
async function writeFile(p: string, content: string) {
  await mkdirp(path.dirname(p));
  await fs.writeFile(p, content);
}

/**
 * Creates a file symlink, first ensuring that the directory to
 * create it into exists.
 */
async function createFileSymlink(target: string, p: string) {
  await mkdirp(path.dirname(p));
  await fs.symlink(target, p, 'file');
}

/**
 * Main entrypoint.
 */
export async function main() {
  config = require('./generate_config.json');
  config.limited_visibility = `@${config.workspace}//:__subpackages__`;

  // get a set of all the direct dependencies for visibility
  const deps = await getDirectDependencySet(config.package_json);

  // find all packages (including packages in nested node_modules)
  const pkgs: Dep[] = [];

  await findPackagesAndPush(pkgs, nodeModulesFolder(), deps);

  // Sort the files to ensure the order
  pkgs.sort(compareDep);

  // flatten dependencies
  flattenDependencies(pkgs);

  // generate Bazel workspaces
  await generateBazelWorkspaces(pkgs);

  // generate all BUILD files
  await generateBuildFiles(pkgs);

  // write a .bazelignore file
  await writeFile('.bazelignore', `node_modules\n${config.workspace_rerooted_path}`);
}

function nodeModulesFolder(): string {
  return config.exports_directories_only ?
    `${config.workspace_rerooted_package_json_dir}/node_modules` :
    'node_modules';
}

/**
 * Generates all build files
 */
async function generateBuildFiles(pkgs: Dep[]) {
  const notNestedPkgs = pkgs.filter(pkg => !pkg._isNested);
  await generateRootBuildFile(notNestedPkgs);
  await notNestedPkgs.reduce((p, pkg) => p.then(() => generatePackageBuildFiles(pkg)), Promise.resolve());
  await (await findScopes()).reduce((prev, scope) => prev.then(() => generateScopeBuildFiles(scope, pkgs)), Promise.resolve());
  // Allow this to overwrite any previously generated BUILD files so that user links take priority
  // over package manager installed npm packages
  await generateLinksBuildFiles(config.links);
}

async function generateLinksBuildFiles(links: {[key: string]: string}) {
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

/**
 * Flattens dependencies on all packages
 */
function flattenDependencies(pkgs: Dep[]) {
  const pkgsMap = new Map();
  pkgs.forEach(pkg => pkgsMap.set(pkg._dir, pkg));
  pkgs.forEach(pkg => flattenPkgDependencies(pkg, pkg, pkgsMap));
}

/**
 * Generates the root BUILD file.
 */
async function generateRootBuildFile(pkgs: Dep[]) {
  let buildFile = config.exports_directories_only ?
      printRootExportsDirectories(pkgs) :
      printRootExportsAllFiles(pkgs);

  // Add the manual build file contents if they exists
  try {
    const manualContents = await fs.readFile(`manual_build_file_contents`, {encoding: 'utf8'});
    buildFile += '\n\n';
    buildFile += manualContents;
  } catch (e) {
  }

  await writeFile('BUILD.bazel', buildFile);
}


function printRootExportsDirectories(pkgs: Dep[]) {
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
`});

let depsStarlark = '';
if (pkgs.length) {
  const list = pkgs.map(pkg => `"//${pkg._dir}:${pkg._name}",`).join('\n        ');
  depsStarlark = `
  deps = [
      ${list}
  ],`;
}

  const result =
    generateBuildFileHeader() + `load("@build_bazel_rules_nodejs//:index.bzl", "js_library")
load("@build_bazel_rules_nodejs//third_party/github.com/bazelbuild/bazel-skylib:rules/copy_file.bzl", "copy_file")

# To support remote-execution, we must create a tree artifacts from the source directories.
# We make the output node_modules/pkg_dir so that we get a free node_modules
# tree in bazel-out and runfiles for this external repository.
${filegroupsStarlark}

# The node_modules directory in one catch-all js_library
js_library(
  name = "node_modules",${depsStarlark}
)`;

  return result
}

function printRootExportsAllFiles(pkgs: Dep[]) {
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
  pkgs.forEach(pkg => {pkg._files.forEach(f => {
                  exportsStarlark += `    "node_modules/${pkg._dir}/${f}",\n`;
                });});

  const result =
      generateBuildFileHeader() + `load("@build_bazel_rules_nodejs//:index.bzl", "js_library")

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

  return result
}

/**
 * Generates all BUILD & bzl files for a package.
 */
async function generatePackageBuildFiles(pkg: Dep) {
  // If a BUILD file was shipped with the package we should symlink the generated BUILD file
  // instead of append its contents to the end of the one we were going to generate.
  // https://github.com/bazelbuild/rules_nodejs/issues/2131
  let buildFilePath: string|undefined;
  if (pkg._files.includes('BUILD')) buildFilePath = 'BUILD';
  if (pkg._files.includes('BUILD.bazel')) buildFilePath = 'BUILD.bazel';

  const nodeModules = nodeModulesFolder();

  // Recreate the pkg dir inside the node_modules folder
  const nodeModulesPkgDir = `${nodeModules}/${pkg._dir}`;
  // Check if the current package dep dir is a symlink (which happens when we
  // install a node_module with link:)
  const isPkgDirASymlink = await fs.lstat(nodeModulesPkgDir)
    .then(stat => stat.isSymbolicLink())
    .catch(constant(false));
  // Mark build file as one to symlink instead of generate as the package dir is a symlink, we
  // have a BUILD file and the pkg is written inside the workspace
  const symlinkBuildFile =
      isPkgDirASymlink && buildFilePath && !config.generate_local_modules_build_files;

  // Log if a BUILD file was expected but was not found
  if (isPkgDirASymlink && !buildFilePath && !config.generate_local_modules_build_files) {
    console.log(`[yarn_install/npm_install]: package ${
        nodeModulesPkgDir} is local symlink and as such a BUILD file for it is expected but none was found. Please add one at ${
        await fs.realpath(nodeModulesPkgDir)}`);
  }

  // The following won't be used in a symlink build file case
  let buildFile = config.exports_directories_only ?
      printPackageExportsDirectories(pkg) :
      printPackageLegacy(pkg);
  if (buildFilePath) {
    buildFile = buildFile + '\n' +
    await fs.readFile(path.join(nodeModules, pkg._dir, buildFilePath), 'utf-8');
  } else {
    buildFilePath = 'BUILD.bazel';
  }

  // if the dependency doesn't appear in the given package.json file, and the 'strict_visibility' flag is set
  // on the npm_install / yarn_install rule, then set the visibility to be limited internally to the @repo workspace
  // if the dependency is listed, set it as public
  // if the flag is false, then always set public visibility
  const visibility = !pkg._directDependency && config.strict_visibility ?
      config.limited_visibility :
      PUBLIC_VISIBILITY;

  // If the package didn't ship a bin/BUILD file, generate one.
  if (!pkg._files.includes('bin/BUILD.bazel') && !pkg._files.includes('bin/BUILD')) {
    const binBuildFile = printPackageBin(pkg);
    if (binBuildFile.length) {
      await writeFile(
          path.posix.join(pkg._dir, 'bin', 'BUILD.bazel'), generateBuildFileHeader(visibility) + binBuildFile);
    }
  }

  // If there's an index.bzl in the package then copy all the package's files
  // other than the BUILD file which we'll write below.
  // (maybe we shouldn't copy .js though, since it belongs under node_modules?)
  const hasIndexBzl = pkg._files.includes('index.bzl')
  if (hasIndexBzl) {
    await pkg._files.filter(f => f !== 'BUILD' && f !== 'BUILD.bazel').reduce(async (prev, file) => {
      if (/^node_modules[/\\]/.test(file)) {
        // don't copy over nested node_modules
        return;
      }
      // don't support rootPath here?
      let destFile = path.posix.join(pkg._dir, file);
      const basename = path.basename(file);
      const basenameUc = basename.toUpperCase();
      // Bazel BUILD files from npm distribution would have been renamed earlier with a _ prefix so
      // we restore the name on the copy
      if (basenameUc === '_BUILD' || basenameUc === '_BUILD.BAZEL') {
        destFile = path.posix.join(path.dirname(destFile), basename.substr(1));
      }
      const src = path.posix.join(nodeModules, pkg._dir, file);
      await prev;
      await mkdirp(path.dirname(destFile));
      await fs.copyFile(src, destFile);
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
      } else {
        buildFile += buildContent;
      }
    }
  

  if (!symlinkBuildFile) {
    await writeFile(
        path.posix.join(pkg._dir, buildFilePath), generateBuildFileHeader(visibility) + buildFile);
  } else {
    const realPathBuildFileForPkg =
        await fs.realpath(path.posix.join(nodeModulesPkgDir, buildFilePath));
    await createFileSymlink(realPathBuildFileForPkg, path.posix.join(pkg._dir, buildFilePath));
  }
}

/**
 * Generate install_<workspace_name>.bzl files with function to install each workspace.
 */
async function generateBazelWorkspaces(pkgs: Dep[]) {
  const workspaces: Bag<string> = {};

  for (const pkg of pkgs) {
    if (!pkg.bazelWorkspaces) {
      continue;
    }

    for (const workspace of Object.keys(pkg.bazelWorkspaces)) {
      // A bazel workspace can only be setup by one npm package
      if (workspaces[workspace]) {
        console.error(
            `Could not setup Bazel workspace ${workspace} requested by npm ` +
            `package ${pkg._dir}@${pkg.version}. Already setup by ${workspaces[workspace]}`);
        process.exit(1);
      }

      await generateBazelWorkspace(pkg, workspace);

      // Keep track of which npm package setup this bazel workspace for later use
      workspaces[workspace] = `${pkg._dir}@${pkg.version}`;
    }
  }
}

/**
 * Generate install_<workspace>.bzl file with function to install the workspace.
 */
async function generateBazelWorkspace(pkg: Dep, workspace: string) {
  let bzlFile = `# Generated by the yarn_install/npm_install rule
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")
load("@build_bazel_rules_nodejs//internal/copy_repository:copy_repository.bzl", "copy_repository")

`;

  const rootPath = pkg.bazelWorkspaces[workspace].rootPath;
  if (!rootPath) {
    console.error(
        `Malformed bazelWorkspaces attribute in ${pkg._dir}@${pkg.version}. ` +
        `Missing rootPath for workspace ${workspace}.`);
    process.exit(1);
  }

  // Copy all files for this workspace to a folder under _workspaces
  // to restore the Bazel files which have be renamed from the npm package
  const workspaceSourcePath = path.posix.join('_workspaces', workspace);
  const nodeModules = nodeModulesFolder();
  await mkdirp(workspaceSourcePath);
  await Promise.all(pkg._files.map(async (file) => {
    if (/^node_modules[/\\]/.test(file)) {
      // don't copy over nested node_modules
      return;
    }
    let destFile = path.relative(rootPath, file);
    if (destFile.startsWith('..')) {
      // this file is not under the rootPath
      return;
    }
    const basename = path.basename(file);
    const basenameUc = basename.toUpperCase();
    // Bazel BUILD files from npm distribution of rules_nodejs 1.x
    // would have been renamed before publishing with a _ prefix so
    // we restore the name on the copy
    if (basenameUc === '_BUILD' || basenameUc === '_BUILD.BAZEL') {
      destFile = path.posix.join(path.dirname(destFile), basename.substr(1));
    }
    const src = path.posix.join(nodeModules, pkg._dir, file);
    const dest = path.posix.join(workspaceSourcePath, destFile);
    await mkdirp(path.dirname(dest));
    await fs.copyFile(src, dest);
  }));

  // We create _bazel_workspace_marker that is used by the custom copy_repository
  // rule to resolve the path to the repository source root. A root BUILD file
  // is required to reference _bazel_workspace_marker as a target so we also create
  // an empty one if one does not exist.
  if (!hasRootBuildFile(pkg, rootPath)) {
    await writeFile(
        path.posix.join(workspaceSourcePath, 'BUILD.bazel'),
        '# Marker file that this directory is a bazel package');
  }
  const sha256sum = crypto.createHash('sha256');
  sha256sum.update(await fs.readFile(config.package_lock, {encoding: 'utf8'}));
  await writeFile(
      path.posix.join(workspaceSourcePath, '_bazel_workspace_marker'),
      `# Marker file to used by custom copy_repository rule\n${sha256sum.digest('hex')}`);

  bzlFile += `def install_${workspace}():
    maybe(
        copy_repository,
        name = "${workspace}",
        marker_file = "@${config.workspace}//_workspaces/${workspace}:_bazel_workspace_marker",
    )
`;

  await writeFile(`install_${workspace}.bzl`, bzlFile);
}

/**
 * Generate build files for a scope.
 */
async function generateScopeBuildFiles(scope: string, pkgs: Dep[]) {
  pkgs = pkgs.filter(pkg => !pkg._isNested && pkg._dir.startsWith(`${scope}/`));
  let deps: Dep[] = [];
  pkgs.forEach(pkg => {
    deps = deps.concat(pkg._dependencies.filter(dep => !dep._isNested && !pkgs.includes(pkg)));
  });
  // filter out duplicate deps
  deps = [...pkgs, ...new Set(deps)];

  let buildFile = config.exports_directories_only ?
      printScopeExportsDirectories(scope, deps) :
      printScopeLegacy(scope, deps);
  await writeFile(path.posix.join(scope, 'BUILD.bazel'), buildFile);
}

/**
 * Checks if a path is a file.
 */
async function isFile(p: string): Promise<boolean> {
  return fs.stat(p)
  .then(stat => stat.isFile())
  .catch(constant(false));
}

/**
 * Checks if a path is an npm package which is is a directory with a package.json file.
 */
async function isDirectory(p: string): Promise<boolean> {
  return fs.stat(p)
    .then((stat) => stat.isDirectory())
    .catch(constant(false));
}

/**
 * Strips the byte order mark from a string if present
 */
function stripBom(s: string) {
  return s.charCodeAt(0) === 0xFEFF ? s.slice(1) : s;
}

/**
 * List all the files under a directory as relative
 * paths to the directory and push them to files.
 */
async function listFilesAndPush(files: string[], rootDir: string, subDir: string = ''): Promise<void> {
  const dir = path.posix.join(rootDir, subDir);
  if (!isDirectory(dir)) {
    return;
  }
  const filelist = await fs.readdir(dir);
  for (const file of filelist) {
    const fullPath = path.posix.join(dir, file);
    const relPath = path.posix.join(subDir, file);
    const isSymbolicLink = (await fs.lstat(fullPath)).isSymbolicLink();
    let stat;
    try {
      stat = await fs.stat(fullPath);
    } catch (e) {
      if (isSymbolicLink) {
        // Filter out broken symbolic links. These cause fs.statSync(fullPath)
        // to fail with `ENOENT: no such file or directory ...`
        if (config.exports_directories_only) {
          // Delete the symlink if we are exporting directory artifacts so the problematic symlink
          // doesn't show up in runfiles. These problematic symlinks cause bazel failures such as
          // ERROR: internal/npm_install/test/BUILD.bazel:118:19:
          //   Testing //internal/npm_install/test:test_yarn_directory_artifacts
          //   failed: Exec failed due to IOException: The file type of
          //   'bazel-out/darwin-fastbuild/bin/internal/npm_install/test/test_yarn_directory_artifacts.sh.runfiles/fine_grained_deps_yarn_directory_artifacts/node_modules/ecstatic/test/public/containsSymlink/problematic'
          //   is not supported.
          await fs.unlink(fullPath);
        }
        continue;
      }
      throw e;
    }
    if (isSymbolicLink && config.exports_directories_only && path.basename(path.dirname(fullPath)) == '.bin') {
      // Delete .bin/* symlinks if exports_directories_only is true since these can point to outside of the
      // tree artifact which breaks that package's copy_directory on linux where symlinks inside TreeArtifacts
      // are verified
      await fs.unlink(fullPath);
      continue
    }
    const isDirectory = stat.isDirectory();
    if (isDirectory && isSymbolicLink) {
      // Filter out symbolic links to directories. An issue in yarn versions
      // older than 1.12.1 creates symbolic links to folders in the .bin folder
      // which leads to Bazel targets that cross package boundaries.
      // See https://github.com/bazelbuild/rules_nodejs/issues/428 and
      // https://github.com/bazelbuild/rules_nodejs/issues/438.
      // This is tested in /e2e/fine_grained_symlinks.
      if (config.exports_directories_only) {
        // Delete the symlink if we are exporting directory artifacts so the problematic symlink
        // doesn't show up in runfiles. These problematic symlinks cause bazel failures such as
        // ERROR: internal/npm_install/test/BUILD.bazel:118:19:
        //   Testing //internal/npm_install/test:test_yarn_directory_artifacts
        //   failed: Exec failed due to IOException: The file type of
        //   'bazel-out/darwin-fastbuild/bin/internal/npm_install/test/test_yarn_directory_artifacts.sh.runfiles/fine_grained_deps_yarn_directory_artifacts/node_modules/ecstatic/test/public/containsSymlink/problematic'
        //   is not supported.
        await fs.unlink(fullPath);
      }
      continue;
    }
    if (isDirectory) await listFilesAndPush(files, rootDir, relPath);
    else files.push(relPath);
  }
}

/**
 * Returns true if the npm package distribution contained a
 * root /BUILD or /BUILD.bazel file.
 */
function hasRootBuildFile(pkg: Dep, rootPath: string) {
  for (const file of pkg._files) {
    const fileUc = path.relative(rootPath, file).toUpperCase();
    if (fileUc === 'BUILD' || fileUc === 'BUILD.BAZEL' ||
        // Also look for the "hidden" version, from older npm packages published
        // by rules_nodejs 1.x
        fileUc === '_BUILD' || fileUc === '_BUILD.BAZEL') {
      return true;
    }
  }
  return false;
}

/**
 * Returns a set of the root package.json files direct dependencies
 */
export async function getDirectDependencySet(pkgJsonPath: string): Promise<Set<string>> {
  const pkgJson = JSON.parse(
    stripBom(await fs.readFile(pkgJsonPath, {encoding: 'utf8'}))
  );

  return new Set([
    ...Object.keys(pkgJson.dependencies || {}),
    ...Object.keys(pkgJson.devDependencies || {}),
    ...Object.keys(pkgJson.optionalDependencies || {}),
  ]);
}

/**
 * Finds all packages under a given path and push to pkgs.
 */
async function findPackagesAndPush(pkgs: Dep[], p: string, dependencies: Set<string>): Promise<void> {
  if (!await isDirectory(p)) {
    return;
  }

  const listing = await fs.readdir(p);

  let promises = listing.map(async f => {
    // filter out folders such as `.bin` which can create
    // issues on Windows since these are "hidden" by default
    if (f.startsWith('.')) return [];
    const pf = path.posix.join(p, f);
    
    if (await isDirectory(pf)) {
      if (f.startsWith('@')) {
        await findPackagesAndPush(pkgs, pf, dependencies);
      } else {
        pkgs.push(await parsePackage(pf, dependencies));
        await findPackagesAndPush(pkgs, path.posix.join(pf, 'node_modules'), dependencies);
      }
    }
  });

  if (config.generate_build_files_concurrency_limit < 1) {
    // unlimited concurrency
    await Promise.all(promises);
  } else {
    while (promises.length) {
      // run batches of generate_build_files_concurrency_limit at a time
      await Promise.all(promises.splice(0, config.generate_build_files_concurrency_limit))
    }
  }
}

/**
 * Finds and returns an array of all package scopes in node_modules.
 */
async function findScopes() {
  const p = nodeModulesFolder();
  if (!await isDirectory(p)) {
    return [];
  }

  const listing = await fs.readdir(p);

  const scopes = (await Promise.all(
    listing.map(async f => {
      if (!f.startsWith('@')) return;
      f = path.posix.join(p, f);
      if (await isDirectory(f)) {
        // strip leading 'node_modules' from filename
        return f.substring(p.length + 1);
      }
    })
  ))
  .filter((f) : f is string => typeof f === 'string');

  return scopes;
}

/**
 * Given the name of a top-level folder in node_modules, parse the
 * package json and return it as an object along with
 * some additional internal attributes prefixed with '_'.
 */
export async function parsePackage(p: string, dependencies: Set<string> = new Set()): Promise<Dep> {
  // Parse the package.json file of this package
  const packageJson = path.posix.join(p, 'package.json');
  const pkg = (await isFile(packageJson)) ?
      JSON.parse(stripBom(await fs.readFile(packageJson, {encoding: 'utf8'}))) :
      {version: '0.0.0'};
  
  // Trim the leading node_modules from the path and
  // assign to _dir for future use
  pkg._dir = p.substring(nodeModulesFolder().length + 1);

  // Stash the package directory name for future use
  pkg._name = pkg._dir.split('/').pop();

  // Module name of the package. Unlike "_name" this represents the
  // full package name (including scope name).
  pkg._moduleName = pkg.name || `${pkg._dir}/${pkg._name}`;

  // Keep track of whether or not this is a nested package
  pkg._isNested = /\/node_modules\//.test(pkg._dir);

  // List all the files in the npm package for later use
  pkg._files = [];

  await listFilesAndPush(pkg._files, p);

  // Sort the files to ensure the order
  pkg._files.sort();

  // The subset of files that are valid in runfiles.
  // Files with spaces (\x20) or unicode characters (<\x20 && >\x7E) are not allowed in
  // Bazel runfiles. See https://github.com/bazelbuild/bazel/issues/4327
  pkg._runfiles = pkg._files.filter((f: string) => !/[^\x21-\x7E]/.test(f));

  // Initialize _dependencies to an empty array
  // which is later filled with the flattened dependency list
  pkg._dependencies = [];

  // set if this is a direct dependency of the root package.json file
  // which is later used to determine the generated rules visibility
  pkg._directDependency = dependencies.has(pkg._moduleName) || dependencies.has(pkg._name) || dependencies.has(pkg._dir);

  return pkg;
}

/**
 * Check if a bin entry is a non-empty path
 */
function isValidBinPath(entry: any) {
  return isValidBinPathStringValue(entry) || isValidBinPathObjectValues(entry);
}

/**
 * If given a string, check if a bin entry is a non-empty path
 */
function isValidBinPathStringValue(entry: any) {
  return typeof entry === 'string' && entry !== '';
}

/**
 * If given an object literal, check if a bin entry objects has at least one a non-empty path
 * Example 1: { entry: './path/to/script.js' } ==> VALID
 * Example 2: { entry: '' } ==> INVALID
 * Example 3: { entry: './path/to/script.js', empty: '' } ==> VALID
 */
function isValidBinPathObjectValues(entry: Bag<string>): boolean {
  // We allow at least one valid entry path (if any).
  return entry && typeof entry === 'object' &&
      Object['values'](entry).filter(_entry => isValidBinPath(_entry)).length > 0;
}

/**
 * Cleanup a package.json "bin" path.
 *
 * Bin paths usually come in 2 flavors: './bin/foo' or 'bin/foo',
 * sometimes other stuff like 'lib/foo'.  Remove prefix './' if it
 * exists.
 */
function cleanupBinPath(p: string) {
  p = p.replace(/\\/g, '/');
  if (p.indexOf('./') === 0) {
    p = p.slice(2);
  }
  return p;
}

/**
 * Cleanup a package.json entry point such as "main"
 *
 * Removes './' if it exists.
 * Appends `index.js` if p ends with `/`.
 */
function cleanupEntryPointPath(p: string) {
  p = p.replace(/\\/g, '/');
  if (p.indexOf('./') === 0) {
    p = p.slice(2);
  }
  if (p.endsWith('/')) {
    p += 'index.js';
  }
  return p;
}

/**
 * Cleans up the given path
 * Then tries to resolve the path into a file and warns if VERBOSE_LOGS set and the file dosen't
 * exist
 * @param {any} pkg
 * @param {string} path
 * @returns {string | undefined}
 */
function findEntryFile(pkg: Dep, path: string) {
  const cleanPath = cleanupEntryPointPath(path);
  // check if main entry point exists
  const entryFile = findFile(pkg, cleanPath) || findFile(pkg, `${cleanPath}.js`);
  if (!entryFile) {
    // If entryPoint entry point listed could not be resolved to a file
    // This can happen
    // in some npm packages that list an incorrect main such as v8-coverage@1.0.8
    // which lists `"main": "index.js"` but that file does not exist.
    log_verbose(
        `could not find entry point for the path ${cleanPath} given by npm package ${pkg._name}`);
  }
  return entryFile;
}

/**
 * Tries to resolve the entryPoint file from the pkg for a given mainFileName
 *
 * @param {any} pkg
 * @param {'browser' | 'module' | 'main'} mainFileName
 * @returns {string | undefined} the path or undefined if we cant resolve the file
 */
function resolveMainFile(pkg: Dep, mainFileName: string) {
  const mainEntryField = pkg[mainFileName];

  if (mainEntryField) {
    if (typeof mainEntryField === 'string') {
      return findEntryFile(pkg, mainEntryField);

    } else if (typeof mainEntryField === 'object' && mainFileName === 'browser') {
      // browser has a weird way of defining this
      // the browser value is an object listing files to alias, usually pointing to a browser dir
      const indexEntryPoint = mainEntryField['index.js'] || mainEntryField['./index.js'];
      if (indexEntryPoint) {
        return findEntryFile(pkg, indexEntryPoint);
      }
    }
  }
}

/**
 * Tries to resolve the mainFile from a given pkg
 * This uses seveal mainFileNames in priority to find a correct usable file
 * @param {any} pkg
 * @returns {string | undefined}
 */
function resolvePkgMainFile(pkg: Dep) {
  // es2015 is another option for mainFile here
  // but its very uncommon and im not sure what priority it takes
  //
  // this list is ordered, we try resolve `browser` first, then `module` and finally fall back to
  // `main`
  const mainFileNames = ['browser', 'module', 'main'];

      for (const mainFile of mainFileNames) {
    const resolvedMainFile = resolveMainFile(pkg, mainFile);
    if (resolvedMainFile) {
      return resolvedMainFile;
    }
  }

  // if we cant find any correct file references from the pkg
  // then we just try looking around for common patterns
  const maybeRootIndex = findEntryFile(pkg, 'index.js');
  if (maybeRootIndex) {
    return maybeRootIndex;
  }

  const maybeSelfNamedIndex = findEntryFile(pkg, `${pkg._name}.js`);
  if (maybeSelfNamedIndex) {
    return maybeSelfNamedIndex;
  }

  // none of the methods we tried resulted in a file
  log_verbose(`could not find entry point for npm package ${pkg._name}`);

  // at this point there's nothing left for us to try, so return nothing
  return undefined;
}

type Bag<T> = Record<string, T>

/**
 * Flattens all transitive dependencies of a package
 * into a _dependencies array.
 */
function flattenPkgDependencies(pkg: Dep, dep: Dep, pkgsMap: Map<string, Dep>) {
  if (pkg._dependencies.indexOf(dep) !== -1) {
    // circular dependency
    return;
  }
  pkg._dependencies.push(dep);
  const findDeps = function(targetDeps: Bag<string>, required: boolean, depType: string) {
    Object.keys(targetDeps || {})
        .map(targetDep => {
          // look for matching nested package
          const dirSegments = dep._dir.split('/');
          while (dirSegments.length) {
            const maybe = path.posix.join(...dirSegments, 'node_modules', targetDep);
            if (pkgsMap.has(maybe)) {
              return pkgsMap.get(maybe);
            }
            dirSegments.pop();
          }
          // look for matching root package
          if (pkgsMap.has(targetDep)) {
            return pkgsMap.get(targetDep);
          }
          // dependency not found
          if (required) {
            console.error(`could not find ${depType} '${targetDep}' of '${dep._dir}'`);
            process.exit(1);
          }
          return null;
        })
        .filter((dep): dep is Dep => Boolean(dep))
        .forEach(dep => flattenPkgDependencies(pkg, dep, pkgsMap));
  };
  // npm will in some cases add optionalDependencies to the list
  // of dependencies to the package.json it writes to node_modules.
  // We delete these here if they exist as they may result
  // in expected dependencies that are not found.
  if (dep.dependencies && dep.optionalDependencies) {
    Object.keys(dep.optionalDependencies).forEach(optionalDep => {
      delete dep.dependencies[optionalDep];
    });
  }

  findDeps(dep.dependencies, true, 'dependency');
  findDeps(dep.peerDependencies, false, 'peer dependency');
  // `optionalDependencies` that are missing should be silently
  // ignored since the npm/yarn will not fail if these dependencies
  // fail to install. Packages should handle the cases where these
  // dependencies are missing gracefully at runtime.
  // An example of this is the `chokidar` package which specifies
  // `fsevents` as an optionalDependency. On OSX, `fsevents`
  // is installed successfully, but on Windows & Linux, `fsevents`
  // fails to install and the package will not be present when
  // checking the dependencies of `chokidar`.
  findDeps(dep.optionalDependencies, false, 'optional dependency');
}

/**
 * Reformat/pretty-print a json object as a skylark comment (each line
 * starts with '# ').
 */
function printJson(pkg: Dep) {
  // Clone and modify _dependencies to avoid circular issues when JSONifying
  // & delete _files & _runfiles arrays
  const cloned: any = {...pkg};
  cloned._dependencies = pkg._dependencies.map(dep => dep._dir);
  delete cloned._files;
  delete cloned._runfiles;
  return JSON.stringify(cloned, null, 2).split('\n').map(line => `# ${line}`).join('\n');
}

/**
 * A filter function for files in an npm package. Comparison is case-insensitive.
 * @param files array of files to filter
 * @param exts list of white listed case-insensitive extensions; if empty, no filter is
 *             done on extensions; '' empty string denotes to allow files with no extensions,
 *             other extensions are listed with '.ext' notation such as '.d.ts'.
 */
function filterFiles(files: string[], exts: string[] = []) {
  if (exts.length) {
    const allowNoExts = exts.includes('');
    files = files.filter(f => {
      // include files with no extensions if noExt is true
      if (allowNoExts && !path.extname(f)) return true;
      // filter files in exts
      const lc = f.toLowerCase();
      for (const e of exts) {
        if (e && lc.endsWith(e.toLowerCase())) {
          return true;
        }
      }
      return false;
    });
  }
  // Filter out BUILD files that came with the npm package
  return files.filter(file => {
    const basenameUc = path.basename(file).toUpperCase();
    // NB: we don't bother filtering out _BUILD or _BUILD.bazel files
    // that might have been published by rules_nodejs 1.x
    if (basenameUc === 'BUILD' || basenameUc === 'BUILD.BAZEL') {
      return false;
    }
    return true;
  });
}

/**
 * Returns true if the specified `pkg` conforms to Angular Package Format (APF),
 * false otherwise. If the package contains `*.metadata.json` and a
 * corresponding sibling `.d.ts` file, then the package is considered to be APF.
 */
function isNgApfPackage(pkg: Dep) {
  const set = new Set(pkg._files);
  if (set.has('ANGULAR_PACKAGE')) {
    // This file is used by the npm/yarn_install rule to detect APF. See
    // https://github.com/bazelbuild/rules_nodejs/issues/927
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

/**
 * Looks for a file within a package and returns it if found.
 */
function findFile(pkg: Dep, m: string) {
  const ml = m.toLowerCase();
  for (const f of pkg._files) {
    if (f.toLowerCase() === ml) {
      return f;
    }
  }
  return undefined;
}

/**
 * Given a pkg, return the skylark `js_library` targets for the package.
 */
function printPackageExportsDirectories(pkg: Dep) {
  // Flattened list of direct and transitive dependencies hoisted to root by the package manager
  const deps = [pkg].concat(pkg._dependencies.filter(dep => dep !== pkg && !dep._isNested));
  const depsStarlark =
      deps.map(dep => `"//:${dep._dir.replace("/", "_")}__contents",`).join('\n        ');

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

/**
 * Given a pkg, return the skylark `js_library` targets for the package.
 */
function printPackageLegacy(pkg: Dep) {
  function starlarkFiles(attr: string, files: string[], comment: string = '') {
    return `
    ${comment ? comment + '\n    ' : ''}${attr} = [
        ${files.map((f: string) => `"//:node_modules/${pkg._dir}/${f}",`).join('\n        ')}
    ],`;
  }

  const includedRunfiles = filterFiles(pkg._runfiles, config.included_files);

  // Files that are part of the npm package not including its nested node_modules
  // (filtered by the 'included_files' attribute)
  const pkgFiles = includedRunfiles.filter((f: string) => !f.startsWith('node_modules/'));
  const pkgFilesStarlark = pkgFiles.length ? starlarkFiles('srcs', pkgFiles) : '';

  // Files that are in the npm package's nested node_modules
  // (filtered by the 'included_files' attribute)
  const nestedNodeModules = includedRunfiles.filter((f: string) => f.startsWith('node_modules/'));
  const nestedNodeModulesStarlark =
      nestedNodeModules.length ? starlarkFiles('srcs', nestedNodeModules) : '';

  // Files that have been excluded from the ${pkg._name}__files target above because
  // they are filtered out by 'included_files' or because they are not valid runfiles
  // See https://github.com/bazelbuild/bazel/issues/4327.
  const notPkgFiles = pkg._files.filter(
      (f: string) => !f.startsWith('node_modules/') && !includedRunfiles.includes(f));
  const notPkgFilesStarlark = notPkgFiles.length ? starlarkFiles('srcs', notPkgFiles) : '';

  // If the package is in the Angular package format returns list
  // of package files that end with `.umd.js`, `.ngfactory.js` and `.ngsummary.js`.
  // TODO(gmagolan): add UMD & AMD scripts to scripts even if not an APF package _but_ only if they
  // are named?
  const namedSources = isNgApfPackage(pkg) ?
      filterFiles(pkg._runfiles, ['.umd.js', '.ngfactory.js', '.ngsummary.js']) :
      [];
  const namedSourcesStarlark = namedSources.length ?
      starlarkFiles(
          'named_module_srcs', namedSources,
          '# subset of srcs that are javascript named-UMD or named-AMD scripts') :
      '';

  // Typings files that are part of the npm package not including nested node_modules
  const dtsSources =
      filterFiles(pkg._runfiles, ['.d.ts']).filter((f: string) => !f.startsWith('node_modules/'));
  const dtsStarlark = dtsSources.length ?
      starlarkFiles(
          'srcs', dtsSources,
          `# ${
              pkg._dir} package declaration files (and declaration files in nested node_modules)`) :
      '';

  // Flattened list of direct and transitive dependencies hoisted to root by the package manager
  const deps = [pkg].concat(pkg._dependencies.filter(dep => dep !== pkg && !dep._isNested));
  const depsStarlark =
      deps.map(dep => `"//${dep._dir}:${dep._name}__contents",`).join('\n        ');

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

  // add an `npm_umd_bundle` target to generate an UMD bundle if one does
  // not exists
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

function _findExecutables(pkg: Dep) {
  const executables = new Map();

  // For root packages, transform the pkg.bin entries
  // into a new Map called _executables
  // NOTE: we do this only for non-empty bin paths
  if (isValidBinPath(pkg.bin)) {
    if (!pkg._isNested) {
      if (Array.isArray(pkg.bin)) {
        if (pkg.bin.length == 1) {
          executables.set(pkg._dir, cleanupBinPath(pkg.bin[0]));
        } else {
          // should not happen, but ignore it if present
        }
      } else if (typeof pkg.bin === 'string') {
        executables.set(pkg._dir, cleanupBinPath(pkg.bin));
      } else if (typeof pkg.bin === 'object') {
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

// Handle additionalAttributes of format:
// ```
// "bazelBin": {
//   "ngc-wrapped": {
//     "additionalAttributes": {
//       "configuration_env_vars": "[\"compile\"]"
//   }
// },
// ```
function additionalAttributes(pkg: Dep, name: string) {
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

/**
 * Given a pkg, return the skylark nodejs_binary targets for the package.
 */
export function printPackageBin(pkg: Dep) {
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

export function printIndexBzl(pkg: Dep) {
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
        npm_package_bin(tool = "@${config.workspace}//${pkg._dir}/bin:${
          name}", output_dir = output_dir, **kwargs)
    else:
        nodejs_binary(
            entry_point = ${entryPoint},
            data = [${data.map(p => `"${p}"`).join(', ')}] + kwargs.pop("data", []),${
          additionalAttributes(pkg, name)}
            **kwargs
        )

# Just in case ${name} is a test runner, also make a test rule for it
def ${name.replace(/-/g, '_')}_test(**kwargs):
    nodejs_test(
      entry_point = ${entryPoint},
      data = [${data.map(p => `"${p}"`).join(', ')}] + kwargs.pop("data", []),${
          additionalAttributes(pkg, name)}
      **kwargs
    )
`;
    }
  }
  return result;
}

type Dep = {
  _dir: string,
  _isNested: boolean,
  _dependencies: Dep[],
  _files: string[],
  _runfiles: string[],
  _directDependency: boolean,
  [k: string]: any
}

/**
 * Given a scope, return the skylark `js_library` target for the scope.
 */
function printScopeLegacy(scope: string, deps: Dep[]) {
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

function printScopeExportsDirectories(scope: string, deps: Dep[]) {
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