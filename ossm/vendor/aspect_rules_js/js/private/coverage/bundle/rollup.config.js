import resolve from '@rollup/plugin-node-resolve'
import commonjs from '@rollup/plugin-commonjs'
import json from '@rollup/plugin-json'

/** @type {import("rollup").RollupOptions} */
export default {
    plugins: [
        {
            transform: (code, id) => {
                const faulty_line = `Cons = require(path.join(__dirname, 'lib', name))`
                const correct = `Cons = require("istanbul-reports/lib/lcovonly")`
                if (
                    id.includes('istanbul-reports') &&
                    code.includes(faulty_line)
                ) {
                    code = code.replace(faulty_line, correct)
                }

                return code
            },
        },
        resolve(),
        commonjs(),
        json(),
    ],
}
