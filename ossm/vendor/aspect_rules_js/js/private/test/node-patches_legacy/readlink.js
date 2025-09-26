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
const assert = require('assert')
const fs = require('fs')
const withFixtures = require('inline-fixtures').withFixtures
const path = require('path')
const util = require('util')

const patcher = require('../../node-patches_legacy/src/fs').patcher

// We don't want to bring jest into this repo so we just fake the describe and it functions here
async function describe(_, fn) {
    await fn()
}
async function it(_, fn) {
    await fn()
}

describe('testing readlink', async () => {
    await it('can resolve symlink in root', async () => {
        await withFixtures(
            {
                a: {},
                b: { file: 'contents' },
            },
            async (fixturesDir) => {
                fixturesDir = fs.realpathSync(fixturesDir)
                // create symlink from a to b
                fs.symlinkSync(
                    path.join(fixturesDir, 'b', 'file'),
                    path.join(fixturesDir, 'a', 'link')
                )

                const patchedFs = Object.assign({}, fs)
                patchedFs.promises = Object.assign({}, fs.promises)

                patcher(patchedFs, [path.join(fixturesDir)])
                const linkPath = path.join(fixturesDir, 'a', 'link')

                assert.deepStrictEqual(
                    patchedFs.readlinkSync(linkPath),
                    path.join(fixturesDir, 'b', 'file'),
                    'SYNC: should read the symlink because its within root'
                )

                assert.deepStrictEqual(
                    patchedFs.readlinkSync(new URL(`file://${linkPath}`)),
                    path.join(fixturesDir, 'b', 'file'),
                    'SYNC: should read the symlink because its within root'
                )

                assert.deepStrictEqual(
                    await util.promisify(patchedFs.readlink)(linkPath),
                    path.join(fixturesDir, 'b', 'file'),
                    'CB: should read the symlink because its within root'
                )

                assert.deepStrictEqual(
                    await patchedFs.promises.readlink(linkPath),
                    path.join(fixturesDir, 'b', 'file'),
                    'Promise: should read the symlink because its within root'
                )
            }
        )
    })

    await it("doesn't resolve as symlink outside of root", async () => {
        await withFixtures(
            {
                a: {},
                b: { file: 'contents' },
            },
            async (fixturesDir) => {
                fixturesDir = fs.realpathSync(fixturesDir)
                // create symlink from a to b
                fs.symlinkSync(
                    path.join(fixturesDir, 'b', 'file'),
                    path.join(fixturesDir, 'a', 'link')
                )

                const patchedFs = Object.assign({}, fs)
                patchedFs.promises = Object.assign({}, fs.promises)

                patcher(patchedFs, [path.join(fixturesDir, 'a')])
                const linkPath = path.join(
                    fs.realpathSync(fixturesDir),
                    'a',
                    'link'
                )

                assert.throws(() => {
                    patchedFs.readlinkSync(linkPath)
                }, "should throw because it's not a link")

                let thrown
                try {
                    await util.promisify(patchedFs.readlink)(linkPath)
                } catch (e) {
                    thrown = e
                } finally {
                    if (!thrown) assert.fail('must throw einval error')
                }

                thrown = undefined
                try {
                    await patchedFs.promises.readlink(linkPath)
                } catch (e) {
                    thrown = e
                } finally {
                    if (!thrown) assert.fail('must throw einval error')
                }
            }
        )
    })

    await it('can resolve symlink to a symlink in the sandbox if it has a corresponding location', async () => {
        await withFixtures(
            {
                sandbox: {},
                execroot: { file: 'contents' },
            },
            async (fixturesDir) => {
                fixturesDir = fs.realpathSync(fixturesDir)

                // create symlink from execroot/link to execroot/file
                fs.symlinkSync(
                    path.join(fixturesDir, 'execroot', 'file'),
                    path.join(fixturesDir, 'execroot', 'link')
                )

                // create sandbox
                fs.symlinkSync(
                    path.join(fixturesDir, 'execroot', 'file'),
                    path.join(fixturesDir, 'sandbox', 'file')
                )
                fs.symlinkSync(
                    path.join(fixturesDir, 'execroot', 'link'),
                    path.join(fixturesDir, 'sandbox', 'link')
                )

                const patchedFs = Object.assign({}, fs)
                patchedFs.promises = Object.assign({}, fs.promises)

                patcher(patchedFs, [path.join(fixturesDir, 'sandbox')])
                const linkPath = path.join(fixturesDir, 'sandbox', 'link')
                const filePath = path.join(fixturesDir, 'sandbox', 'file')

                assert.deepStrictEqual(
                    patchedFs.readlinkSync(linkPath),
                    filePath,
                    'SYNC: should read the symlink in the sandbox'
                )

                assert.deepStrictEqual(
                    patchedFs.readlinkSync(new URL(`file://${linkPath}`)),
                    filePath,
                    'SYNC: should read the symlink in the sandbox'
                )

                assert.deepStrictEqual(
                    await util.promisify(patchedFs.readlink)(linkPath),
                    filePath,
                    'CB: should read the symlink in the sandbox'
                )

                assert.deepStrictEqual(
                    await patchedFs.promises.readlink(linkPath),
                    filePath,
                    'Promise: should read the symlink in the sandbox'
                )
            }
        )
    })

    await it('cant resolve symlink to a symlink in the sandbox if it has no corresponding location', async () => {
        await withFixtures(
            {
                sandbox: {},
                execroot: {},
                otherroot: { file: 'contents' },
            },
            async (fixturesDir) => {
                fixturesDir = fs.realpathSync(fixturesDir)

                // create dangling symlink from execroot/link to execroot/file
                fs.symlinkSync(
                    path.join(fixturesDir, 'otherroot', 'file'),
                    path.join(fixturesDir, 'execroot', 'link')
                )
                // create sandbox
                fs.symlinkSync(
                    path.join(fixturesDir, 'execroot', 'link'),
                    path.join(fixturesDir, 'sandbox', 'link')
                )

                const patchedFs = Object.assign({}, fs)
                patchedFs.promises = Object.assign({}, fs.promises)

                patcher(patchedFs, [path.join(fixturesDir, 'sandbox')])
                const linkPath = path.join(fixturesDir, 'sandbox', 'link')
                const filePath = path.join(fixturesDir, 'sandbox', 'file')

                assert.throws(() => {
                    patchedFs.readlinkSync(linkPath)
                }, "should throw because it's not a resolvable link")

                let thrown
                try {
                    await util.promisify(patchedFs.readlink)(linkPath)
                } catch (e) {
                    thrown = e
                } finally {
                    if (!thrown) assert.fail('must throw einval error')
                }

                thrown = undefined
                try {
                    await patchedFs.promises.readlink(linkPath)
                } catch (e) {
                    thrown = e
                } finally {
                    if (!thrown) assert.fail('must throw einval error')
                }
            }
        )
    })

    await it('includes parent calls in stack traces', async function readlinkStackTest1() {
        let err
        try {
            fs.readlinkSync(null)
        } catch (e) {
            err = e
        } finally {
            if (!err) assert.fail('readlinkSync should fail on invalid path')
            if (!err.stack.includes('readlinkStackTest1'))
                assert.fail(
                    `readlinkSync error stack should contain calling method: ${err.stack}`
                )
        }
    })
})
