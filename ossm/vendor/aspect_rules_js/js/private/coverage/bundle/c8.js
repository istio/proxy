import { Report } from 'c8'
import fs from 'fs'
import path from 'path'

// bazel will create the COVERAGE_OUTPUT_FILE whilst setting up the sandbox.
// therefore, should be doing a file size check rather than presence.
try {
    const stats = fs.statSync(process.env.COVERAGE_OUTPUT_FILE)
    if (stats.size != 0) {
        // early exit here does not affect the outcome of the tests.
        // bazel will only execute _lcov_merger when tests pass.
        process.exit(0)
    }
    // in case file doesn't exist or some other error is thrown, just ignore it.
} catch {}

const include = fs
    .readFileSync(process.env.COVERAGE_MANIFEST)
    .toString('utf8')
    .split('\n')
    .filter((f) => f != '')

// TODO: can or should we instrument files from other repositories as well?
// if so then the path.join call below will yield invalid paths since files will have external/wksp as their prefix.
const pwd = path.join(process.env.RUNFILES, process.env.TEST_WORKSPACE)
process.chdir(pwd)

new Report({
    include: include,
    exclude: include.length === 0 ? ['**'] : [],
    reportsDirectory: process.env.COVERAGE_DIR,
    tempDirectory: process.env.COVERAGE_DIR,
    resolve: '',
    src: pwd,
    all: true,
    reporter: ['lcovonly'],
})
    .run()
    .then(() => {
        fs.renameSync(
            path.join(process.env.COVERAGE_DIR, 'lcov.info'),
            process.env.COVERAGE_OUTPUT_FILE
        )
    })
    .catch((err) => {
        console.error(err)
        process.exit(1)
    })
