// clang-format off
'use strict';

var path = require('path');
var util = require('util');
var fs$1 = require('fs');

function _interopDefaultLegacy (e) { return e && typeof e === 'object' && 'default' in e ? e : { 'default': e }; }

var path__default = /*#__PURE__*/_interopDefaultLegacy(path);
var util__default = /*#__PURE__*/_interopDefaultLegacy(util);
var fs__default = /*#__PURE__*/_interopDefaultLegacy(fs$1);

var commonjsGlobal = typeof globalThis !== 'undefined' ? globalThis : typeof window !== 'undefined' ? window : typeof global !== 'undefined' ? global : typeof self !== 'undefined' ? self : {};

function createCommonjsModule(fn) {
  var module = { exports: {} };
	return fn(module, module.exports), module.exports;
}

var fs = createCommonjsModule(function (module, exports) {
/**
 * @license
 * Copyright 2019 The Bazel Authors. All rights reserved.
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
var __asyncValues = (commonjsGlobal && commonjsGlobal.__asyncValues) || function (o) {
    if (!Symbol.asyncIterator) throw new TypeError("Symbol.asyncIterator is not defined.");
    var m = o[Symbol.asyncIterator], i;
    return m ? m.call(o) : (o = typeof __values === "function" ? __values(o) : o[Symbol.iterator](), i = {}, verb("next"), verb("throw"), verb("return"), i[Symbol.asyncIterator] = function () { return this; }, i);
    function verb(n) { i[n] = o[n] && function (v) { return new Promise(function (resolve, reject) { v = o[n](v), settle(resolve, reject, v.done, v.value); }); }; }
    function settle(resolve, reject, d, v) { Promise.resolve(v).then(function(v) { resolve({ value: v, done: d }); }, reject); }
};
var __await = (commonjsGlobal && commonjsGlobal.__await) || function (v) { return this instanceof __await ? (this.v = v, this) : new __await(v); };
var __asyncGenerator = (commonjsGlobal && commonjsGlobal.__asyncGenerator) || function (thisArg, _arguments, generator) {
    if (!Symbol.asyncIterator) throw new TypeError("Symbol.asyncIterator is not defined.");
    var g = generator.apply(thisArg, _arguments || []), i, q = [];
    return i = {}, verb("next"), verb("throw"), verb("return"), i[Symbol.asyncIterator] = function () { return this; }, i;
    function verb(n) { if (g[n]) i[n] = function (v) { return new Promise(function (a, b) { q.push([n, v, a, b]) > 1 || resume(n, v); }); }; }
    function resume(n, v) { try { step(g[n](v)); } catch (e) { settle(q[0][3], e); } }
    function step(r) { r.value instanceof __await ? Promise.resolve(r.value.v).then(fulfill, reject) : settle(q[0][2], r); }
    function fulfill(value) { resume("next", value); }
    function reject(value) { resume("throw", value); }
    function settle(f, v) { if (f(v), q.shift(), q.length) resume(q[0][0], q[0][1]); }
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.escapeFunction = exports.isOutPath = exports.patcher = void 0;


// using require here on purpose so we can override methods with any
// also even though imports are mutable in typescript the cognitive dissonance is too high because
// es modules

// tslint:disable-next-line:no-any
const patcher = (fs = fs__default['default'], roots) => {
    fs = fs || fs__default['default'];
    roots = roots || [];
    roots = roots.filter(root => fs.existsSync(root));
    if (!roots.length) {
        if (process.env.VERBOSE_LOGS) {
            console.error('fs patcher called without any valid root paths ' + __filename);
        }
        return;
    }
    const origRealpath = fs.realpath.bind(fs);
    const origRealpathNative = fs.realpath.native;
    const origLstat = fs.lstat.bind(fs);
    const origStat = fs.stat.bind(fs);
    const origStatSync = fs.statSync.bind(fs);
    const origReadlink = fs.readlink.bind(fs);
    const origLstatSync = fs.lstatSync.bind(fs);
    const origRealpathSync = fs.realpathSync.bind(fs);
    const origRealpathSyncNative = fs.realpathSync.native;
    const origReadlinkSync = fs.readlinkSync.bind(fs);
    const origReaddir = fs.readdir.bind(fs);
    const origReaddirSync = fs.readdirSync.bind(fs);
    const { isEscape } = exports.escapeFunction(roots);
    // tslint:disable-next-line:no-any
    fs.lstat = (...args) => {
        let cb = args.length > 1 ? args[args.length - 1] : undefined;
        // preserve error when calling function without required callback.
        if (cb) {
            cb = once(cb);
            args[args.length - 1] = (err, stats) => {
                if (err)
                    return cb(err);
                path__default['default'].resolve(args[0]);
                if (!stats.isSymbolicLink()) {
                    return cb(null, stats);
                }
                return origReadlink(args[0], (err, str) => {
                    if (err) {
                        if (err.code === 'ENOENT') {
                            return cb(null, stats);
                        }
                        else if (err.code === 'EINVAL') {
                            // readlink only returns einval when the target is not a link.
                            // so if we found a link and it's no longer a link someone raced file system
                            // modifications. we return the error but a strong case could be made to return the
                            // original stat.
                            return cb(err);
                        }
                        else {
                            // some other file system related error.
                            return cb(err);
                        }
                    }
                    str = path__default['default'].resolve(path__default['default'].dirname(args[0]), str);
                    if (isEscape(str, args[0])) {
                        // if it's an out link we have to return the original stat.
                        return origStat(args[0], (err, plainStat) => {
                            if (err && err.code === 'ENOENT') {
                                // broken symlink. return link stats.
                                return cb(null, stats);
                            }
                            cb(err, plainStat);
                        });
                    }
                    // its a symlink and its inside of the root.
                    cb(null, stats);
                });
            };
        }
        origLstat(...args);
    };
    // tslint:disable-next-line:no-any
    fs.realpath = (...args) => {
        let cb = args.length > 1 ? args[args.length - 1] : undefined;
        if (cb) {
            cb = once(cb);
            args[args.length - 1] = (err, str) => {
                if (err)
                    return cb(err);
                if (isEscape(str, args[0])) {
                    cb(null, path__default['default'].resolve(args[0]));
                }
                else {
                    cb(null, str);
                }
            };
        }
        origRealpath(...args);
    };
    fs.realpath.native =
        (...args) => {
            let cb = args.length > 1 ? args[args.length - 1] : undefined;
            if (cb) {
                cb = once(cb);
                args[args.length - 1] = (err, str) => {
                    if (err)
                        return cb(err);
                    if (isEscape(str, args[0])) {
                        cb(null, path__default['default'].resolve(args[0]));
                    }
                    else {
                        cb(null, str);
                    }
                };
            }
            origRealpathNative(...args);
        };
    // tslint:disable-next-line:no-any
    fs.readlink = (...args) => {
        let cb = args.length > 1 ? args[args.length - 1] : undefined;
        if (cb) {
            cb = once(cb);
            args[args.length - 1] = (err, str) => {
                args[0] = path__default['default'].resolve(args[0]);
                if (str)
                    str = path__default['default'].resolve(path__default['default'].dirname(args[0]), str);
                if (err)
                    return cb(err);
                if (isEscape(str, args[0])) {
                    const e = new Error('EINVAL: invalid argument, readlink \'' + args[0] + '\'');
                    // tslint:disable-next-line:no-any
                    e.code = 'EINVAL';
                    // if its not supposed to be a link we have to trigger an EINVAL error.
                    return cb(e);
                }
                cb(null, str);
            };
        }
        origReadlink(...args);
    };
    // tslint:disable-next-line:no-any
    fs.lstatSync = (...args) => {
        const stats = origLstatSync(...args);
        const linkPath = path__default['default'].resolve(args[0]);
        if (!stats.isSymbolicLink()) {
            return stats;
        }
        let linkTarget;
        try {
            linkTarget = path__default['default'].resolve(path__default['default'].dirname(args[0]), origReadlinkSync(linkPath));
        }
        catch (e) {
            if (e.code === 'ENOENT') {
                return stats;
            }
            throw e;
        }
        if (isEscape(linkTarget, linkPath)) {
            try {
                return origStatSync(...args);
            }
            catch (e) {
                // enoent means we have a broken link.
                // broken links that escape are returned as lstat results
                if (e.code !== 'ENOENT') {
                    throw e;
                }
            }
        }
        return stats;
    };
    // tslint:disable-next-line:no-any
    fs.realpathSync = (...args) => {
        const str = origRealpathSync(...args);
        if (isEscape(str, args[0])) {
            return path__default['default'].resolve(args[0]);
        }
        return str;
    };
    // tslint:disable-next-line:no-any
    fs.realpathSync.native = (...args) => {
        const str = origRealpathSyncNative(...args);
        if (isEscape(str, args[0])) {
            return path__default['default'].resolve(args[0]);
        }
        return str;
    };
    // tslint:disable-next-line:no-any
    fs.readlinkSync = (...args) => {
        args[0] = path__default['default'].resolve(args[0]);
        const str = path__default['default'].resolve(path__default['default'].dirname(args[0]), origReadlinkSync(...args));
        if (isEscape(str, args[0]) || str === args[0]) {
            const e = new Error('EINVAL: invalid argument, readlink \'' + args[0] + '\'');
            // tslint:disable-next-line:no-any
            e.code = 'EINVAL';
            throw e;
        }
        return str;
    };
    // tslint:disable-next-line:no-any
    fs.readdir = (...args) => {
        const p = path__default['default'].resolve(args[0]);
        let cb = args[args.length - 1];
        if (typeof cb !== 'function') {
            // this will likely throw callback required error.
            return origReaddir(...args);
        }
        cb = once(cb);
        args[args.length - 1] = (err, result) => {
            if (err)
                return cb(err);
            // user requested withFileTypes
            if (result[0] && result[0].isSymbolicLink) {
                Promise.all(result.map((v) => handleDirent(p, v)))
                    .then(() => {
                    cb(null, result);
                })
                    .catch(err => {
                    cb(err);
                });
            }
            else {
                // string array return for readdir.
                cb(null, result);
            }
        };
        origReaddir(...args);
    };
    // tslint:disable-next-line:no-any
    fs.readdirSync = (...args) => {
        const res = origReaddirSync(...args);
        const p = path__default['default'].resolve(args[0]);
        // tslint:disable-next-line:no-any
        res.forEach((v) => {
            handleDirentSync(p, v);
        });
        return res;
    };
    // i need to use this twice in bodt readdor and readdirSync. maybe in fs.Dir
    // tslint:disable-next-line:no-any
    function patchDirent(dirent, stat) {
        // add all stat is methods to Dirent instances with their result.
        for (const i in stat) {
            if (i.indexOf('is') === 0 && typeof stat[i] === 'function') {
                //
                const result = stat[i]();
                if (result)
                    dirent[i] = () => true;
                else
                    dirent[i] = () => false;
            }
        }
    }
    if (fs.opendir) {
        const origOpendir = fs.opendir.bind(fs);
        // tslint:disable-next-line:no-any
        fs.opendir = (...args) => {
            let cb = args[args.length - 1];
            // if this is not a function opendir should throw an error.
            // we call it so we don't have to throw a mock
            if (typeof cb === 'function') {
                cb = once(cb);
                args[args.length - 1] = async (err, dir) => {
                    try {
                        cb(null, await handleDir(dir));
                    }
                    catch (e) {
                        cb(e);
                    }
                };
                origOpendir(...args);
            }
            else {
                return origOpendir(...args).then((dir) => {
                    return handleDir(dir);
                });
            }
        };
    }
    async function handleDir(dir) {
        const p = path__default['default'].resolve(dir.path);
        const origIterator = dir[Symbol.asyncIterator].bind(dir);
        // tslint:disable-next-line:no-any
        const origRead = dir.read.bind(dir);
        dir[Symbol.asyncIterator] = function () {
            return __asyncGenerator(this, arguments, function* () {
                var e_1, _a;
                try {
                    for (var _b = __asyncValues(origIterator()), _c; _c = yield __await(_b.next()), !_c.done;) {
                        const entry = _c.value;
                        yield __await(handleDirent(p, entry));
                        yield yield __await(entry);
                    }
                }
                catch (e_1_1) { e_1 = { error: e_1_1 }; }
                finally {
                    try {
                        if (_c && !_c.done && (_a = _b.return)) yield __await(_a.call(_b));
                    }
                    finally { if (e_1) throw e_1.error; }
                }
            });
        };
        // tslint:disable-next-line:no-any
        dir.read = async (...args) => {
            if (typeof args[args.length - 1] === 'function') {
                const cb = args[args.length - 1];
                args[args.length - 1] = async (err, entry) => {
                    cb(err, entry ? await handleDirent(p, entry) : null);
                };
                origRead(...args);
            }
            else {
                const entry = await origRead(...args);
                if (entry) {
                    await handleDirent(p, entry);
                }
                return entry;
            }
        };
        // tslint:disable-next-line:no-any
        const origReadSync = dir.readSync.bind(dir);
        // tslint:disable-next-line:no-any
        dir.readSync = () => {
            return handleDirentSync(p, origReadSync());
        };
        return dir;
    }
    let handleCounter = 0;
    function handleDirent(p, v) {
        handleCounter++;
        return new Promise((resolve, reject) => {
            if (fs.DEBUG)
                console.error(handleCounter + ' opendir: found link? ', path__default['default'].join(p, v.name), v.isSymbolicLink());
            if (!v.isSymbolicLink()) {
                return resolve(v);
            }
            const linkName = path__default['default'].join(p, v.name);
            origReadlink(linkName, (err, target) => {
                if (err) {
                    return reject(err);
                }
                if (fs.DEBUG)
                    console.error(handleCounter + ' opendir: escapes? [target]', path__default['default'].resolve(target), '[link] ' + linkName, isEscape(path__default['default'].resolve(target), linkName), roots);
                if (!isEscape(path__default['default'].resolve(target), linkName)) {
                    return resolve(v);
                }
                fs.stat(target, (err, stat) => {
                    if (err) {
                        if (err.code === 'ENOENT') {
                            if (fs.DEBUG)
                                console.error(handleCounter + ' opendir: broken link! resolving to link ', path__default['default'].resolve(target));
                            // this is a broken symlink
                            // even though this broken symlink points outside of the root
                            // we'll return it.
                            // the alternative choice here is to omit it from the directory listing altogether
                            // this would add complexity because readdir output would be different than readdir
                            // withFileTypes unless readdir was changed to match. if readdir was changed to match
                            // it's performance would be greatly impacted because we would always have to use the
                            // withFileTypes version which is slower.
                            return resolve(v);
                        }
                        // transient fs related error. busy etc.
                        return reject(err);
                    }
                    if (fs.DEBUG)
                        console.error(handleCounter + ' opendir: patching dirent to look like it\'s target', path__default['default'].resolve(target));
                    // add all stat is methods to Dirent instances with their result.
                    patchDirent(v, stat);
                    v.isSymbolicLink = () => false;
                    resolve(v);
                });
            });
        });
    }
    function handleDirentSync(p, v) {
        if (v && v.isSymbolicLink) {
            if (v.isSymbolicLink()) {
                // any errors thrown here are valid. things like transient fs errors
                const target = path__default['default'].resolve(p, origReadlinkSync(path__default['default'].join(p, v.name)));
                if (isEscape(target, path__default['default'].join(p, v.name))) {
                    // Dirent exposes file type so if we want to hide that this is a link
                    // we need to find out if it's a file or directory.
                    v.isSymbolicLink = () => false;
                    // tslint:disable-next-line:no-any
                    const stat = origStatSync(target);
                    // add all stat is methods to Dirent instances with their result.
                    patchDirent(v, stat);
                }
            }
        }
    }
    /**
     * patch fs.promises here.
     *
     * this requires a light touch because if we trigger the getter on older nodejs versions
     * it will log an experimental warning to stderr
     *
     * `(node:62945) ExperimentalWarning: The fs.promises API is experimental`
     *
     * this api is available as experimental without a flag so users can access it at any time.
     */
    const promisePropertyDescriptor = Object.getOwnPropertyDescriptor(fs, 'promises');
    if (promisePropertyDescriptor) {
        // tslint:disable-next-line:no-any
        const promises = {};
        promises.lstat = util__default['default'].promisify(fs.lstat);
        // NOTE: node core uses the newer realpath function fs.promises.native instead of fs.realPath
        promises.realpath = util__default['default'].promisify(fs.realpath.native);
        promises.readlink = util__default['default'].promisify(fs.readlink);
        promises.readdir = util__default['default'].promisify(fs.readdir);
        if (fs.opendir)
            promises.opendir = util__default['default'].promisify(fs.opendir);
        // handle experimental api warnings.
        // only applies to version of node where promises is a getter property.
        if (promisePropertyDescriptor.get) {
            const oldGetter = promisePropertyDescriptor.get.bind(fs);
            const cachedPromises = {};
            promisePropertyDescriptor.get = () => {
                const _promises = oldGetter();
                Object.assign(cachedPromises, _promises, promises);
                return cachedPromises;
            };
            Object.defineProperty(fs, 'promises', promisePropertyDescriptor);
        }
        else {
            // api can be patched directly
            Object.assign(fs.promises, promises);
        }
    }
};
exports.patcher = patcher;
function isOutPath(root, str) {
    if (!root)
        return true;
    let strParts = str.split(path__default['default'].sep);
    let rootParts = root.split(path__default['default'].sep);
    let i = 0;
    for (; i < rootParts.length && i < strParts.length; i++) {
        if (rootParts[i] === strParts[i] || rootParts[i] === '*') {
            continue;
        }
        break;
    }
    return i < rootParts.length;
}
exports.isOutPath = isOutPath;
const escapeFunction = (roots) => {
    // ensure roots are always absolute
    roots = roots.map(root => path__default['default'].resolve(root));
    function isEscape(linkTarget, linkPath) {
        if (!path__default['default'].isAbsolute(linkPath)) {
            linkPath = path__default['default'].resolve(linkPath);
        }
        if (!path__default['default'].isAbsolute(linkTarget)) {
            linkTarget = path__default['default'].resolve(linkTarget);
        }
        for (const root of roots) {
            if (isOutPath(root, linkTarget) && !isOutPath(root, linkPath)) {
                // don't escape out of the root
                return true;
            }
        }
        return false;
    }
    return { isEscape, isOutPath };
};
exports.escapeFunction = escapeFunction;
function once(fn) {
    let called = false;
    return (...args) => {
        if (called)
            return;
        called = true;
        let err = false;
        try {
            fn(...args);
        }
        catch (_e) {
            err = _e;
        }
        // blow the stack to make sure this doesn't fall into any unresolved promise contexts
        if (err) {
            setImmediate(() => {
                throw err;
            });
        }
    };
}
});

var subprocess = createCommonjsModule(function (module, exports) {
Object.defineProperty(exports, "__esModule", { value: true });
exports.patcher = void 0;
// this does not actually patch child_process
// but adds support to ensure the registered loader is included in all nested executions of nodejs.


const patcher = (requireScriptName, nodeDir) => {
    requireScriptName = path__default['default'].resolve(requireScriptName);
    nodeDir = nodeDir || path__default['default'].join(path__default['default'].dirname(requireScriptName), '_node_bin');
    const file = path__default['default'].basename(requireScriptName);
    try {
        fs__default['default'].mkdirSync(nodeDir, { recursive: true });
    }
    catch (e) {
        // with node versions that don't have recursive mkdir this may throw an error.
        if (e.code !== 'EEXIST') {
            throw e;
        }
    }
    if (process.platform == 'win32') {
        const nodeEntry = path__default['default'].join(nodeDir, 'node.bat');
        if (!fs__default['default'].existsSync(nodeEntry)) {
            fs__default['default'].writeFileSync(nodeEntry, `@if not defined DEBUG_HELPER @ECHO OFF
set NP_SUBPROCESS_NODE_DIR=${nodeDir}
set Path=${nodeDir};%Path%
"${process.execPath}" --require "${requireScriptName}" %*
`);
        }
    }
    else {
        const nodeEntry = path__default['default'].join(nodeDir, 'node');
        if (!fs__default['default'].existsSync(nodeEntry)) {
            fs__default['default'].writeFileSync(nodeEntry, `#!/bin/bash
export NP_SUBPROCESS_NODE_DIR="${nodeDir}"
export PATH="${nodeDir}":\$PATH
if [[ ! "\${@}" =~ "${file}" ]]; then
  exec ${process.execPath} --require "${requireScriptName}" "$@"
else
  exec ${process.execPath} "$@"
fi
`, { mode: 0o777 });
        }
    }
    if (!process.env.PATH) {
        process.env.PATH = nodeDir;
    }
    else if (process.env.PATH.indexOf(nodeDir + path__default['default'].delimiter) === -1) {
        process.env.PATH = nodeDir + path__default['default'].delimiter + process.env.PATH;
    }
    // fix execPath so folks use the proxy node
    if (process.platform == 'win32') ;
    else {
        process.argv[0] = process.execPath = path__default['default'].join(nodeDir, 'node');
    }
    // replace any instances of require script in execArgv with the absolute path to the script.
    // example: bazel-require-script.js
    process.execArgv.map(v => {
        if (v.indexOf(file) > -1) {
            return requireScriptName;
        }
        return v;
    });
};
exports.patcher = patcher;
});

var src = createCommonjsModule(function (module, exports) {
Object.defineProperty(exports, "__esModule", { value: true });
exports.subprocess = exports.fs = void 0;
/**
 * @license
 * Copyright 2019 The Bazel Authors. All rights reserved.
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


exports.fs = fs.patcher;
exports.subprocess = subprocess.patcher;
});

/**
 * @license
 * Copyright 2019 The Bazel Authors. All rights reserved.
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
 * @fileoverview Description of this file.
 */

const { BAZEL_PATCH_ROOTS, NP_SUBPROCESS_NODE_DIR, VERBOSE_LOGS } = process.env;
if (BAZEL_PATCH_ROOTS) {
    const roots = BAZEL_PATCH_ROOTS ? BAZEL_PATCH_ROOTS.split(',') : [];
    if (VERBOSE_LOGS)
        console.error(`bazel node patches enabled. roots: ${roots} symlinks in these directories will not escape`);
    const fs = fs__default['default'];
    src.fs(fs, roots);
}
else if (VERBOSE_LOGS) {
    console.error(`bazel node patches disabled. set environment BAZEL_PATCH_ROOTS`);
}
src.subprocess(__filename, NP_SUBPROCESS_NODE_DIR);

var register = {

};

module.exports = register;
