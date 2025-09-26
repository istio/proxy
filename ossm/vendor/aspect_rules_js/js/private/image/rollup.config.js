import resolve from '@rollup/plugin-node-resolve'
import commonjs from '@rollup/plugin-commonjs'
import ts from '@rollup/plugin-typescript'

/** @type {import("rollup").RollupOptions} */
export default {
    plugins: [
        resolve(),
        commonjs(),
        ts({
            sourceMap: true,
            inlineSourceMap: true,
            target: 'es2022',
        }),
    ],
    output: {
        sourcemap: 'inline',
    },
}
