/* THIS FILE GENERATED FROM .ts; see BUILD.bazel */ /* clang-format off */"use strict";
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.main = exports.reduceModules = void 0;
const fs = require("fs");
const path = require("path");
const { runfiles: _defaultRunfiles, _BAZEL_OUT_REGEX } = require('../runfiles/index.cjs');
const VERBOSE_LOGS = !!process.env['VERBOSE_LOGS'];
function log_verbose(...m) {
    if (VERBOSE_LOGS)
        console.error('[link_node_modules.js]', ...m);
}
function log_error(error) {
    console.error('[link_node_modules.js] An error has been reported:', error, error.stack);
}
function mkdirp(p) {
    return __awaiter(this, void 0, void 0, function* () {
        if (p && !(yield exists(p))) {
            yield mkdirp(path.dirname(p));
            log_verbose(`creating directory ${p} in ${process.cwd()}`);
            try {
                yield fs.promises.mkdir(p);
            }
            catch (e) {
                if (e.code !== 'EEXIST') {
                    throw e;
                }
            }
        }
    });
}
function gracefulLstat(path) {
    return __awaiter(this, void 0, void 0, function* () {
        try {
            return yield fs.promises.lstat(path);
        }
        catch (e) {
            if (e.code === 'ENOENT') {
                return null;
            }
            throw e;
        }
    });
}
function gracefulReadlink(path) {
    try {
        return fs.readlinkSync(path);
    }
    catch (e) {
        if (e.code === 'ENOENT') {
            return null;
        }
        throw e;
    }
}
function gracefulReaddir(path) {
    return __awaiter(this, void 0, void 0, function* () {
        try {
            return yield fs.promises.readdir(path);
        }
        catch (e) {
            if (e.code === 'ENOENT') {
                return [];
            }
            throw e;
        }
    });
}
function unlink(moduleName) {
    return __awaiter(this, void 0, void 0, function* () {
        const stat = yield gracefulLstat(moduleName);
        if (stat === null) {
            return;
        }
        log_verbose(`unlink( ${moduleName} )`);
        if (stat.isDirectory()) {
            yield deleteDirectory(moduleName);
        }
        else {
            log_verbose("Deleting file: ", moduleName);
            yield fs.promises.unlink(moduleName);
        }
    });
}
function deleteDirectory(p) {
    return __awaiter(this, void 0, void 0, function* () {
        log_verbose("Deleting children of", p);
        for (let entry of yield gracefulReaddir(p)) {
            const childPath = path.join(p, entry);
            const stat = yield gracefulLstat(childPath);
            if (stat === null) {
                log_verbose(`File does not exist, but is listed as directory entry: ${childPath}`);
                continue;
            }
            if (stat.isDirectory()) {
                yield deleteDirectory(childPath);
            }
            else {
                log_verbose("Deleting file", childPath);
                yield fs.promises.unlink(childPath);
            }
        }
        log_verbose("Cleaning up dir", p);
        yield fs.promises.rmdir(p);
    });
}
function symlink(target, p) {
    return __awaiter(this, void 0, void 0, function* () {
        if (!path.isAbsolute(target)) {
            target = path.resolve(process.cwd(), target);
        }
        log_verbose(`creating symlink ${p} -> ${target}`);
        try {
            yield fs.promises.symlink(target, p, 'junction');
            return true;
        }
        catch (e) {
            if (e.code !== 'EEXIST') {
                throw e;
            }
            if (VERBOSE_LOGS) {
                if (!(yield exists(p))) {
                    log_verbose('ERROR\n***\nLooks like we created a bad symlink:' +
                        `\n  pwd ${process.cwd()}\n  target ${target}\n  path ${p}\n***`);
                }
            }
            return false;
        }
    });
}
function resolveWorkspaceNodeModules(externalWorkspace, startCwd, isExecroot, execroot, runfiles) {
    return __awaiter(this, void 0, void 0, function* () {
        const targetManifestPath = `${externalWorkspace}/node_modules`;
        if (isExecroot) {
            return `${execroot}/external/${targetManifestPath}`;
        }
        if (!execroot) {
            return path.resolve(`${startCwd}/../${targetManifestPath}`);
        }
        const fromManifest = runfiles.resolve(targetManifestPath);
        if (fromManifest) {
            return fromManifest;
        }
        else {
            const maybe = path.resolve(`${execroot}/external/${targetManifestPath}`);
            if (yield exists(maybe)) {
                return maybe;
            }
            return path.resolve(`${startCwd}/../${targetManifestPath}`);
        }
    });
}
function exists(p) {
    return __awaiter(this, void 0, void 0, function* () {
        return ((yield gracefulLstat(p)) !== null);
    });
}
function existsSync(p) {
    if (!p) {
        return false;
    }
    try {
        fs.lstatSync(p);
        return true;
    }
    catch (e) {
        if (e.code === 'ENOENT') {
            return false;
        }
        throw e;
    }
}
function reduceModules(modules) {
    return buildModuleHierarchy(Object.keys(modules).sort(), modules, '/').children || [];
}
exports.reduceModules = reduceModules;
function buildModuleHierarchy(moduleNames, modules, elementPath) {
    let element = {
        name: elementPath.slice(0, -1),
        link: modules[elementPath.slice(0, -1)],
        children: [],
    };
    for (let i = 0; i < moduleNames.length;) {
        const moduleName = moduleNames[i];
        const next = moduleName.indexOf('/', elementPath.length + 1);
        const moduleGroup = (next === -1) ? (moduleName + '/') : moduleName.slice(0, next + 1);
        if (next === -1) {
            i++;
        }
        const siblings = [];
        while (i < moduleNames.length && moduleNames[i].startsWith(moduleGroup)) {
            siblings.push(moduleNames[i++]);
        }
        let childElement = buildModuleHierarchy(siblings, modules, moduleGroup);
        for (let cur = childElement; (cur = liftElement(childElement)) !== childElement;) {
            childElement = cur;
        }
        element.children.push(childElement);
    }
    if (!element.link) {
        delete element.link;
    }
    if (!element.children || element.children.length === 0) {
        delete element.children;
    }
    return element;
}
function liftElement(element) {
    let { name, link, children } = element;
    if (!children || !children.length) {
        return element;
    }
    if (link && allElementsAlignUnder(name, link, children)) {
        return { name, link };
    }
    return element;
}
function allElementsAlignUnder(parentName, parentLink, elements) {
    for (const { name, link, children } of elements) {
        if (!link || children) {
            return false;
        }
        if (!isDirectChildPath(parentName, name)) {
            return false;
        }
        if (!isDirectChildLink(parentLink, link)) {
            return false;
        }
        if (!isNameLinkPathTopAligned(name, link)) {
            return false;
        }
    }
    return true;
}
function isDirectChildPath(parent, child) {
    return parent === path.dirname(child);
}
function isDirectChildLink(parentLink, childLink) {
    return parentLink === path.dirname(childLink);
}
function isNameLinkPathTopAligned(namePath, linkPath) {
    return path.basename(namePath) === path.basename(linkPath);
}
function visitDirectoryPreserveLinks(dirPath, visit) {
    return __awaiter(this, void 0, void 0, function* () {
        for (const entry of yield fs.promises.readdir(dirPath)) {
            const childPath = path.join(dirPath, entry);
            const stat = yield gracefulLstat(childPath);
            if (stat === null) {
                continue;
            }
            if (stat.isDirectory()) {
                yield visitDirectoryPreserveLinks(childPath, visit);
            }
            else {
                yield visit(childPath, stat);
            }
        }
    });
}
function findExecroot(startCwd) {
    if (existsSync(`${startCwd}/bazel-out`)) {
        return startCwd;
    }
    const bazelOutMatch = startCwd.match(_BAZEL_OUT_REGEX);
    return bazelOutMatch ? startCwd.slice(0, bazelOutMatch.index) : undefined;
}
function main(args, runfiles) {
    return __awaiter(this, void 0, void 0, function* () {
        if (!args || args.length < 1)
            throw new Error('requires one argument: modulesManifest path');
        const [modulesManifest] = args;
        log_verbose('manifest file:', modulesManifest);
        let { workspace, bin, roots, module_sets } = JSON.parse(fs.readFileSync(modulesManifest));
        log_verbose('manifest contents:', JSON.stringify({ workspace, bin, roots, module_sets }, null, 2));
        roots = roots || {};
        module_sets = module_sets || {};
        const startCwd = process.cwd().replace(/\\/g, '/');
        log_verbose('startCwd:', startCwd);
        const execroot = findExecroot(startCwd);
        log_verbose('execroot:', execroot ? execroot : 'not found');
        const isExecroot = startCwd == execroot;
        log_verbose('isExecroot:', isExecroot.toString());
        const isBazelRun = !!process.env['BUILD_WORKSPACE_DIRECTORY'];
        log_verbose('isBazelRun:', isBazelRun.toString());
        if (!isExecroot && execroot) {
            process.chdir(execroot);
            log_verbose('changed directory to execroot', execroot);
        }
        function symlinkWithUnlink(target, p, stats = null) {
            return __awaiter(this, void 0, void 0, function* () {
                if (!path.isAbsolute(target)) {
                    target = path.resolve(process.cwd(), target);
                }
                if (stats === null) {
                    stats = yield gracefulLstat(p);
                }
                if (runfiles.manifest && execroot && stats !== null && stats.isSymbolicLink()) {
                    const symlinkPathRaw = gracefulReadlink(p);
                    if (symlinkPathRaw !== null) {
                        const symlinkPath = symlinkPathRaw.replace(/\\/g, '/');
                        if (path.relative(symlinkPath, target) != '' &&
                            !path.relative(execroot, symlinkPath).startsWith('..')) {
                            log_verbose(`Out-of-date symlink for ${p} to ${symlinkPath} detected. Target should be ${target}. Unlinking.`);
                            yield unlink(p);
                        }
                        else {
                            log_verbose(`The symlink at ${p} no longer exists, so no need to unlink it.`);
                        }
                    }
                }
                return symlink(target, p);
            });
        }
        for (const packagePath of Object.keys(roots)) {
            const externalWorkspace = roots[packagePath];
            let workspaceNodeModules = yield resolveWorkspaceNodeModules(externalWorkspace, startCwd, isExecroot, execroot, runfiles);
            if (yield exists(workspaceNodeModules)) {
                log_verbose(`resolved ${externalWorkspace} external workspace node modules path to ${workspaceNodeModules}`);
            }
            else {
                workspaceNodeModules = undefined;
            }
            let primaryNodeModules;
            if (packagePath) {
                const binNodeModules = path.posix.join(bin, packagePath, 'node_modules');
                yield mkdirp(path.dirname(binNodeModules));
                if (workspaceNodeModules) {
                    yield symlinkWithUnlink(workspaceNodeModules, binNodeModules);
                    primaryNodeModules = workspaceNodeModules;
                }
                else {
                    yield mkdirp(binNodeModules);
                    primaryNodeModules = binNodeModules;
                }
                if (!isBazelRun) {
                    const execrootNodeModules = path.posix.join(packagePath, 'node_modules');
                    yield mkdirp(path.dirname(execrootNodeModules));
                    yield symlinkWithUnlink(primaryNodeModules, execrootNodeModules);
                }
            }
            else {
                const execrootNodeModules = 'node_modules';
                if (workspaceNodeModules) {
                    yield symlinkWithUnlink(workspaceNodeModules, execrootNodeModules);
                    primaryNodeModules = workspaceNodeModules;
                }
                else {
                    yield mkdirp(execrootNodeModules);
                    primaryNodeModules = execrootNodeModules;
                }
            }
            if (!isExecroot) {
                const runfilesNodeModules = path.posix.join(startCwd, packagePath, 'node_modules');
                yield mkdirp(path.dirname(runfilesNodeModules));
                yield symlinkWithUnlink(primaryNodeModules, runfilesNodeModules);
            }
            if (process.env['RUNFILES']) {
                const stat = yield gracefulLstat(process.env['RUNFILES']);
                if (stat && stat.isDirectory()) {
                    const runfilesNodeModules = path.posix.join(process.env['RUNFILES'], workspace, 'node_modules');
                    yield mkdirp(path.dirname(runfilesNodeModules));
                    yield symlinkWithUnlink(primaryNodeModules, runfilesNodeModules);
                }
            }
        }
        function isLeftoverDirectoryFromLinker(stats, modulePath) {
            return __awaiter(this, void 0, void 0, function* () {
                if (runfiles.manifest === undefined) {
                    return false;
                }
                if (!stats.isDirectory()) {
                    return false;
                }
                let isLeftoverFromPreviousLink = true;
                yield visitDirectoryPreserveLinks(modulePath, (childPath, childStats) => __awaiter(this, void 0, void 0, function* () {
                    if (!childStats.isSymbolicLink()) {
                        isLeftoverFromPreviousLink = false;
                    }
                }));
                return isLeftoverFromPreviousLink;
            });
        }
        function createSymlinkAndPreserveContents(stats, modulePath, target) {
            return __awaiter(this, void 0, void 0, function* () {
                const tmpPath = `${modulePath}__linker_tmp`;
                log_verbose(`createSymlinkAndPreserveContents( ${modulePath} )`);
                yield symlink(target, tmpPath);
                yield visitDirectoryPreserveLinks(modulePath, (childPath, stat) => __awaiter(this, void 0, void 0, function* () {
                    if (stat.isSymbolicLink()) {
                        const targetPath = path.join(tmpPath, path.relative(modulePath, childPath));
                        log_verbose(`Cloning symlink into temporary created link ( ${childPath} )`);
                        yield mkdirp(path.dirname(targetPath));
                        yield symlink(targetPath, yield fs.promises.realpath(childPath));
                    }
                }));
                log_verbose(`Removing existing module so that new link can take place ( ${modulePath} )`);
                yield unlink(modulePath);
                yield fs.promises.rename(tmpPath, modulePath);
            });
        }
        function linkModules(package_path, m) {
            return __awaiter(this, void 0, void 0, function* () {
                const symlinkIn = package_path ?
                    path.posix.join(bin, package_path, 'node_modules') :
                    'node_modules';
                if (path.dirname(m.name)) {
                    yield mkdirp(`${symlinkIn}/${path.dirname(m.name)}`);
                }
                if (m.link) {
                    const modulePath = m.link;
                    let target;
                    if (isExecroot) {
                        target = `${startCwd}/${modulePath}`;
                    }
                    if (!isExecroot || !existsSync(target)) {
                        let runfilesPath = modulePath;
                        if (runfilesPath.startsWith(`${bin}/`)) {
                            runfilesPath = runfilesPath.slice(bin.length + 1);
                        }
                        else if (runfilesPath === bin) {
                            runfilesPath = '';
                        }
                        const externalPrefix = 'external/';
                        if (runfilesPath.startsWith(externalPrefix)) {
                            runfilesPath = runfilesPath.slice(externalPrefix.length);
                        }
                        else {
                            runfilesPath = path.posix.join(workspace, runfilesPath);
                        }
                        try {
                            target = runfiles.resolve(runfilesPath);
                            if (runfiles.manifest && modulePath.startsWith(`${bin}/`)) {
                                if (!target.match(_BAZEL_OUT_REGEX)) {
                                    const e = new Error(`could not resolve module ${runfilesPath} in output tree`);
                                    e.code = 'MODULE_NOT_FOUND';
                                    throw e;
                                }
                            }
                        }
                        catch (err) {
                            target = undefined;
                            log_verbose(`runfiles resolve failed for module '${m.name}': ${err.message}`);
                        }
                    }
                    if (target && !path.isAbsolute(target)) {
                        target = path.resolve(process.cwd(), target);
                    }
                    const symlinkFile = `${symlinkIn}/${m.name}`;
                    const stats = yield gracefulLstat(symlinkFile);
                    const isLeftOver = (stats !== null && (yield isLeftoverDirectoryFromLinker(stats, symlinkFile)));
                    if (target && (yield exists(target))) {
                        if (stats !== null && isLeftOver) {
                            yield createSymlinkAndPreserveContents(stats, symlinkFile, target);
                        }
                        else {
                            yield symlinkWithUnlink(target, symlinkFile, stats);
                        }
                    }
                    else {
                        if (!target) {
                            log_verbose(`no symlink target found for module ${m.name}`);
                        }
                        else {
                            log_verbose(`potential target ${target} does not exists for module ${m.name}`);
                        }
                        if (isLeftOver) {
                            yield unlink(symlinkFile);
                        }
                    }
                }
                if (m.children) {
                    yield Promise.all(m.children.map(m => linkModules(package_path, m)));
                }
            });
        }
        const links = [];
        for (const package_path of Object.keys(module_sets)) {
            const modules = module_sets[package_path];
            log_verbose(`modules for package path '${package_path}':\n${JSON.stringify(modules, null, 2)}`);
            const moduleHierarchy = reduceModules(modules);
            log_verbose(`mapping hierarchy for package path '${package_path}':\n${JSON.stringify(moduleHierarchy)}`);
            links.push(...moduleHierarchy.map(m => linkModules(package_path, m)));
        }
        let code = 0;
        yield Promise.all(links).catch(e => {
            log_error(e);
            code = 1;
        });
        return code;
    });
}
exports.main = main;
if (require.main === module) {
    if (Number(process.versions.node.split('.')[0]) < 10) {
        console.error(`ERROR: rules_nodejs linker requires Node v10 or greater, but is running on ${process.versions.node}`);
        console.error('Note that earlier Node versions are no longer in long-term-support, see');
        console.error('https://nodejs.org/en/about/releases/');
        process.exit(1);
    }
    (() => __awaiter(void 0, void 0, void 0, function* () {
        try {
            process.exitCode = yield main(process.argv.slice(2), _defaultRunfiles);
        }
        catch (e) {
            log_error(e);
            process.exitCode = 1;
        }
    }))();
}
