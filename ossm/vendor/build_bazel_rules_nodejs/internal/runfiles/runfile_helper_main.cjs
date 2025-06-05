/**
 * File that re-exports the runfile helpers. This is the entry-point for the runfile helper
 * script that can be accessed through the `BAZEL_NODE_RUNFILES_HELPER` environment variable.
 *
 * ```ts
 *   require(process.env['BAZEL_NODE_RUNFILES_HELPER'])
 * ```
 */
module.exports = require('./index.cjs').runfiles;
