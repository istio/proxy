const nodeResolve = require("@rollup/plugin-node-resolve");
const commonjs = require("@rollup/plugin-commonjs");
const replace = require("@rollup/plugin-replace");
const json = require("@rollup/plugin-json");

module.exports = {
    external: ['readable-stream'],
    plugins: [
        nodeResolve({
            preferBuiltins: true
        }),
        // https://github.com/rollup/rollup/issues/1507#issuecomment-340550539
        replace({
            preventAssignment: true,
            delimiters: ['', ''],
            values: {
                'readable-stream': 'stream'
            }
        }),
        commonjs(), json(),
    ],
    input: "lifecycle-hooks.js",
    output: {
        file: "min/index.js",
        format: "cjs",
    },
    onwarn: (warning, defaultHandler) => {
        // warning but works, hide it. https://github.com/isaacs/node-glob/issues/365
        if (warning.code === "CIRCULAR_DEPENDENCY" && warning.message.includes('/glob/')) {
            return;
        }
        defaultHandler(warning);
    },
};