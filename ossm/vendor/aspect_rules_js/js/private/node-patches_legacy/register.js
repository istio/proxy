const patchfs = require('./fs').patcher
const {
    JS_BINARY__FS_PATCH_ROOTS,
    JS_BINARY__LOG_DEBUG,
    JS_BINARY__LOG_PREFIX,
    JS_BINARY__NODE_WRAPPER,
    JS_BINARY__PATCH_NODE_FS,
} = process.env

// Keep a count of how many times these patches are applied; this should reflect the depth
// of child processes in the default case where a child process inherits process.env since
// child processes need to re-apply the patches. This is here primarily for testing but it
// could also be useful for debugging.
if (!process.env.JS_BINARY__NODE_PATCHES_DEPTH) {
    process.env.JS_BINARY__NODE_PATCHES_DEPTH = '.'
} else {
    process.env.JS_BINARY__NODE_PATCHES_DEPTH += '.'
}

// subprocess patch
if (process.platform == 'win32') {
    // FIXME: need to make an exe, or run in a shell so we can use .bat
} else {
    if (JS_BINARY__LOG_DEBUG) {
        console.error(
            `DEBUG: ${JS_BINARY__LOG_PREFIX}: overriding process.execPath to node wrapper path ${JS_BINARY__NODE_WRAPPER}`
        )
    }
    process.argv[0] = process.execPath = JS_BINARY__NODE_WRAPPER
}

// fs patches
if (
    JS_BINARY__PATCH_NODE_FS &&
    JS_BINARY__PATCH_NODE_FS != '0' &&
    JS_BINARY__FS_PATCH_ROOTS
) {
    const fs = require('node:fs')
    const module = require('node:module')
    const roots = JS_BINARY__FS_PATCH_ROOTS.split(':')
    if (JS_BINARY__LOG_DEBUG) {
        console.error(
            `DEBUG: ${JS_BINARY__LOG_PREFIX}: node fs patches will be applied with roots: ${roots}`
        )
    }
    patchfs(fs, roots)

    // Sync the esm modules to use the now patched fs cjs module.
    // See: https://nodejs.org/api/esm.html#builtin-modules
    module.syncBuiltinESMExports()
}
