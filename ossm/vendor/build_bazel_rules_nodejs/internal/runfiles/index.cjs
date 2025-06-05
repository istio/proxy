// clang-format off
'use strict';

var path = require('path');
var fs = require('fs');

function _interopDefaultLegacy (e) { return e && typeof e === 'object' && 'default' in e ? e : { 'default': e }; }

var path__default = /*#__PURE__*/_interopDefaultLegacy(path);
var fs__default = /*#__PURE__*/_interopDefaultLegacy(fs);

function getDefaultExportFromCjs (x) {
	return x && x.__esModule && Object.prototype.hasOwnProperty.call(x, 'default') ? x['default'] : x;
}

function createCommonjsModule(fn) {
  var module = { exports: {} };
	return fn(module, module.exports), module.exports;
}

var paths = createCommonjsModule(function (module, exports) {
Object.defineProperty(exports, "__esModule", { value: true });
exports.BAZEL_OUT_REGEX = void 0;
// NB: on windows thanks to legacy 8-character path segments it might be like
// c:/b/ojvxx6nx/execroot/build_~1/bazel-~1/x64_wi~1/bin/internal/npm_in~1/test
exports.BAZEL_OUT_REGEX = /(\/bazel-out\/|\/bazel-~1\/x64_wi~1\/)/;
});

var runfiles$1 = createCommonjsModule(function (module, exports) {
Object.defineProperty(exports, "__esModule", { value: true });
exports.Runfiles = void 0;



/**
 * Class that provides methods for resolving Bazel runfiles.
 */
class Runfiles {
    constructor(_env) {
        this._env = _env;
        // If Bazel sets a variable pointing to a runfiles manifest,
        // we'll always use it.
        // Note that this has a slight performance implication on Mac/Linux
        // where we could use the runfiles tree already laid out on disk
        // but this just costs one file read for the external npm/node_modules
        // and one for each first-party module, not one per file.
        if (!!_env['RUNFILES_MANIFEST_FILE']) {
            this.manifest = this.loadRunfilesManifest(_env['RUNFILES_MANIFEST_FILE']);
        }
        else if (!!_env['RUNFILES_DIR']) {
            this.runfilesDir = path__default['default'].resolve(_env['RUNFILES_DIR']);
        }
        else if (!!_env['RUNFILES']) {
            this.runfilesDir = path__default['default'].resolve(_env['RUNFILES']);
        }
        else {
            throw new Error('Every node program run under Bazel must have a $RUNFILES_DIR, $RUNFILES or $RUNFILES_MANIFEST_FILE environment variable');
        }
        // Under --noenable_runfiles (in particular on Windows)
        // Bazel sets RUNFILES_MANIFEST_ONLY=1.
        // When this happens, we need to read the manifest file to locate
        // inputs
        if (_env['RUNFILES_MANIFEST_ONLY'] === '1' && !_env['RUNFILES_MANIFEST_FILE']) {
            console.warn(`Workaround https://github.com/bazelbuild/bazel/issues/7994
                 RUNFILES_MANIFEST_FILE should have been set but wasn't.
                 falling back to using runfiles symlinks.
                 If you want to test runfiles manifest behavior, add
                 --spawn_strategy=standalone to the command line.`);
        }
        // Bazel starts actions with pwd=execroot/my_wksp or pwd=runfiles/my_wksp
        this.workspace = _env['BAZEL_WORKSPACE'] || _env['JS_BINARY__WORKSPACE'] || undefined;
        // If target is from an external workspace such as @npm//rollup/bin:rollup
        // resolvePackageRelative is not supported since package is in an external
        // workspace.
        let target = _env['BAZEL_TARGET'] || _env['JS_BINARY__TARGET'];
        if (!!target && !target.startsWith('@')) {
            // //path/to:target -> path/to
            this.package = target.split(':')[0].replace(/^\/\//, '');
        }
    }
    /** Resolves the given path from the runfile manifest. */
    _resolveFromManifest(searchPath) {
        if (!this.manifest)
            return undefined;
        let result;
        for (const [k, v] of this.manifest) {
            // Account for Bazel --legacy_external_runfiles
            // which pollutes the workspace with 'my_wksp/external/...'
            if (k.startsWith(`${searchPath}/external`))
                continue;
            // If the manifest entry fully matches, return the value path without
            // considering other manifest entries. We already have an exact match.
            if (k === searchPath) {
                return v;
            }
            // Consider a case where `npm/node_modules` is resolved, and we have the following
            // manifest: `npm/node_modules/semver/LICENSE /path/to/external/npm/node_modules/semver/LICENSE`
            // To resolve the directory, we look for entries that either fully match, or refer to contents
            // within the directory we are looking for. We can then subtract the child path to resolve the
            // directory. e.g. in the case above we subtract `length(`/semver/LICENSE`)` from the entry value.
            if (k.startsWith(`${searchPath}/`)) {
                const l = k.length - searchPath.length;
                const maybe = v.substring(0, v.length - l);
                if (maybe.match(paths.BAZEL_OUT_REGEX)) {
                    return maybe;
                }
                else {
                    result = maybe;
                }
            }
        }
        return result;
    }
    /**
     * The runfiles manifest maps from short_path
     * https://docs.bazel.build/versions/main/skylark/lib/File.html#short_path
     * to the actual location on disk where the file can be read.
     *
     * In a sandboxed execution, it does not exist. In that case, runfiles must be
     * resolved from a symlink tree under the runfiles dir.
     * See https://github.com/bazelbuild/bazel/issues/3726
     */
    loadRunfilesManifest(manifestPath) {
        const runfilesEntries = new Map();
        const input = fs__default['default'].readFileSync(manifestPath, { encoding: 'utf-8' });
        for (const line of input.split('\n')) {
            if (!line)
                continue;
            const [runfilesPath, realPath] = line.split(' ');
            runfilesEntries.set(runfilesPath, realPath);
        }
        return runfilesEntries;
    }
    /** Resolves the given module path. */
    resolve(modulePath) {
        // Normalize path by converting to forward slashes and removing all trailing
        // forward slashes
        modulePath = modulePath.replace(/\\/g, '/').replace(/\/+$/g, '');
        if (path__default['default'].isAbsolute(modulePath)) {
            return modulePath;
        }
        const result = this._resolve(modulePath, undefined);
        if (result) {
            return result;
        }
        const e = new Error(`could not resolve module ${modulePath}`);
        e.code = 'MODULE_NOT_FOUND';
        throw e;
    }
    /** Resolves the given path relative to the current Bazel workspace. */
    resolveWorkspaceRelative(modulePath) {
        // Normalize path by converting to forward slashes and removing all trailing
        // forward slashes
        modulePath = modulePath.replace(/\\/g, '/').replace(/\/+$/g, '');
        if (!this.workspace) {
            throw new Error('workspace could not be determined from the environment; make sure BAZEL_WORKSPACE is set');
        }
        return this.resolve(path__default['default'].posix.join(this.workspace, modulePath));
    }
    /** Resolves the given path relative to the current Bazel package. */
    resolvePackageRelative(modulePath) {
        // Normalize path by converting to forward slashes and removing all trailing
        // forward slashes
        modulePath = modulePath.replace(/\\/g, '/').replace(/\/+$/g, '');
        if (!this.workspace) {
            throw new Error('workspace could not be determined from the environment; make sure BAZEL_WORKSPACE is set');
        }
        // NB: this.package may be '' if at the root of the workspace
        if (this.package === undefined) {
            throw new Error('package could not be determined from the environment; make sure BAZEL_TARGET is set');
        }
        return this.resolve(path__default['default'].posix.join(this.workspace, this.package, modulePath));
    }
    /**
     * Patches the default NodeJS resolution to support runfile resolution.
     * @deprecated Use the runfile helpers directly instead.
     **/
    patchRequire() {
        const requirePatch = this._env['BAZEL_NODE_PATCH_REQUIRE'];
        if (!requirePatch) {
            throw new Error('require patch location could not be determined from the environment');
        }
        require(requirePatch);
    }
    /** Helper for resolving a given module recursively in the runfiles. */
    _resolve(moduleBase, moduleTail) {
        if (this.manifest) {
            const result = this._resolveFromManifest(moduleBase);
            if (result) {
                if (moduleTail) {
                    const maybe = path__default['default'].join(result, moduleTail || '');
                    if (fs__default['default'].existsSync(maybe)) {
                        return maybe;
                    }
                }
                else {
                    return result;
                }
            }
        }
        if (this.runfilesDir) {
            const maybe = path__default['default'].join(this.runfilesDir, moduleBase, moduleTail || '');
            if (fs__default['default'].existsSync(maybe)) {
                return maybe;
            }
        }
        const dirname = path__default['default'].dirname(moduleBase);
        if (dirname == '.') {
            // no match
            return undefined;
        }
        return this._resolve(dirname, path__default['default'].join(path__default['default'].basename(moduleBase), moduleTail || ''));
    }
}
exports.Runfiles = Runfiles;
});

var runfiles = createCommonjsModule(function (module, exports) {
Object.defineProperty(exports, "__esModule", { value: true });
exports.runfiles = exports._BAZEL_OUT_REGEX = exports.Runfiles = void 0;

Object.defineProperty(exports, "Runfiles", { enumerable: true, get: function () { return runfiles$1.Runfiles; } });

Object.defineProperty(exports, "_BAZEL_OUT_REGEX", { enumerable: true, get: function () { return paths.BAZEL_OUT_REGEX; } });
/** Instance of the runfile helpers. */
exports.runfiles = new runfiles$1.Runfiles(process.env);
});

var index = /*@__PURE__*/getDefaultExportFromCjs(runfiles);

module.exports = index;
