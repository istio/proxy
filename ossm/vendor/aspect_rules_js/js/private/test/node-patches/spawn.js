const child_process = require('child_process')

const printbinjs = process.argv[2]
const printbinsh = process.argv[3]
const printdepthjs = process.argv[4]
const printdepthsh = process.argv[5]

// We don't want to bring jest into this repo so we just fake the describe and it functions here
async function describe(_, fn) {
    await fn()
}
async function it(_, fn) {
    await fn()
}

describe('child_process node path', async () => {
    function assertNodePath({ stdout, stderr, code, error }) {
        // Process errors
        if (stderr?.toString().trim()) {
            throw new Error(`Received stderr: ${stderr.toString()}`)
        } else if (code) {
            throw new Error(`Exit code: ${code}`)
        } else if (error) {
            throw new Error(`Error: ${error}`)
        }

        const childNodePath = stdout.toString().trim()
        const expectedNodePath = process.execPath

        // NdoeJS path
        if (childNodePath !== expectedNodePath) {
            throw new Error(
                `Expected: ${expectedNodePath}\n Actual: ${childNodePath}`
            )
        }
    }

    function testNodePathAsync(cp) {
        return new Promise((resolve, reject) => {
            let stdout = '',
                stderr = ''
            cp.stdout.on('data', (moreOut) => (stdout += moreOut))
            cp.stderr.on('data', (moreError) => (stderr += moreError))

            cp.on('error', reject)
            cp.on('close', (code) => {
                try {
                    resolve(assertNodePath({ stdout, stderr, code }))
                } catch (e) {
                    reject(e)
                }
            })
        })
    }

    function testNodePathSync(cp) {
        return assertNodePath(cp)
    }

    function createAssertNodePathCallback(resolve, reject) {
        return (error, stdout, stderr) => {
            try {
                resolve(assertNodePath({ stdout, stderr, error }))
            } catch (e) {
                reject(e)
            }
        }
    }

    await it('should launch patched node via child_process.execSync("node")', () => {
        testNodePathSync({
            stdout: child_process.execSync(`node ${printbinjs}`),
        })
    })

    await it('should launch patched node via child_process.spawnSync("node")', () => {
        testNodePathSync(child_process.spawnSync('node', [printbinjs]))
    })

    await it('should launch patched node via child_process.spawn("node")', async () => {
        await testNodePathAsync(child_process.spawn('node', [printbinjs]))
    })

    await it('should launch patched node via child_process.spawn("node") with {shell: true}', async () => {
        await testNodePathAsync(
            child_process.spawn('node', [printbinjs], { shell: true })
        )
    })

    await it('should launch patched node via child_process.fork()', async () => {
        await testNodePathAsync(
            child_process.fork(printbinjs, { stdio: 'pipe' })
        )
    })

    await it('should launch patched node via child_process.exec("node")', async () => {
        await new Promise((resolve, reject) =>
            child_process.exec(
                `node ${printbinjs}`,
                createAssertNodePathCallback(resolve, reject)
            )
        )
    })

    await it('should return patched node via exec(`which node`)', async () => {
        await new Promise((resolve, reject) =>
            child_process.exec(
                'which node',
                createAssertNodePathCallback(resolve, reject)
            )
        )
    })

    await it('should launch patched node via child_process.execFile()', async () => {
        await new Promise((resolve, reject) =>
            child_process.execFile(
                printbinsh,
                createAssertNodePathCallback(resolve, reject)
            )
        )
    })

    await it('should launch patched node via child_process.execFileSync()', () => {
        testNodePathSync({ stdout: child_process.execFileSync(printbinsh) })
    })
})

describe('child_process patch depth', async () => {
    function assertPatchDepth({ stdout, stderr, code, error }) {
        // Process errors
        if (stderr?.toString().trim()) {
            throw new Error(`Received stderr: ${stderr.toString()}`)
        } else if (code) {
            throw new Error(`Exit code: ${code}`)
        } else if (error) {
            throw new Error(`Error: ${error}`)
        }

        const expectedPatchDepth =
            process.env.JS_BINARY__NODE_PATCHES_DEPTH + '.'
        const childPatchDepth = stdout.toString().trim()

        // NdoeJS path
        if (childPatchDepth !== expectedPatchDepth) {
            throw new Error(
                `Expected: ${expectedPatchDepth}\n Actual: ${childPatchDepth}`
            )
        }
    }

    function testPatchDepthAsync(cp) {
        return new Promise((resolve, reject) => {
            let stdout = '',
                stderr = ''
            cp.stdout.on('data', (moreOut) => (stdout += moreOut))
            cp.stderr.on('data', (moreError) => (stderr += moreError))

            cp.on('error', reject)
            cp.on('close', (code) => {
                try {
                    resolve(assertPatchDepth({ stdout, stderr, code }))
                } catch (e) {
                    reject(e)
                }
            })
        })
    }

    function testPatchDepthSync(cp) {
        return assertPatchDepth(cp)
    }

    function createAssertPatchDepthCallback(resolve, reject) {
        return (error, stdout, stderr) => {
            try {
                resolve(assertPatchDepth({ stdout, stderr, error }))
            } catch (e) {
                reject(e)
            }
        }
    }

    await it('should launch patched node via child_process.execSync("node")', () => {
        testPatchDepthSync({
            stdout: child_process.execSync(`node ${printdepthjs}`),
        })
    })

    await it('should launch patched node via child_process.spawnSync("node")', () => {
        testPatchDepthSync(child_process.spawnSync('node', [printdepthjs]))
    })

    await it('should launch patched node via child_process.spawn("node")', async () => {
        await testPatchDepthAsync(child_process.spawn('node', [printdepthjs]))
    })

    await it('should launch patched node via child_process.spawn("node") with {shell: true}', async () => {
        await testPatchDepthAsync(
            child_process.spawn('node', [printdepthjs], { shell: true })
        )
    })

    await it('should launch patched node via child_process.fork()', async () => {
        await testPatchDepthAsync(
            child_process.fork(printdepthjs, { stdio: 'pipe' })
        )
    })

    await it('should launch patched node via child_process.exec("node")', async () => {
        await new Promise((resolve, reject) =>
            child_process.exec(
                `node ${printdepthjs}`,
                createAssertPatchDepthCallback(resolve, reject)
            )
        )
    })

    await it('should return patched node via node -e "<program>"', async () => {
        await new Promise((resolve, reject) =>
            child_process.exec(
                'node -e "console.log(process.env.JS_BINARY__NODE_PATCHES_DEPTH)"',
                createAssertPatchDepthCallback(resolve, reject)
            )
        )
    })

    await it('should launch patched node via child_process.execFile()', async () => {
        await new Promise((resolve, reject) =>
            child_process.execFile(
                printdepthsh,
                createAssertPatchDepthCallback(resolve, reject)
            )
        )
    })

    await it('should launch patched node via child_process.execFileSync()', () => {
        testPatchDepthSync({ stdout: child_process.execFileSync(printdepthsh) })
    })
})
