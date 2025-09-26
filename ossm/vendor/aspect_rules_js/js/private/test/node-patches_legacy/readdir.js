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

describe('testing readdir', async () => {
    await it('can readdir dirent in root', async () => {
        await withFixtures(
            {
                a: { apples: 'contents' },
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
                patcher(patchedFs, [fixturesDir])

                let dirents = patchedFs.readdirSync(
                    path.join(fixturesDir, 'a'),
                    {
                        withFileTypes: true,
                    }
                )
                assert.deepStrictEqual(dirents[0].name, 'apples')
                assert.deepStrictEqual(dirents[1].name, 'link')
                assert.ok(dirents[0].isFile())
                assert.ok(dirents[1].isSymbolicLink())

                dirents = await util.promisify(patchedFs.readdir)(
                    path.join(fixturesDir, 'a'),
                    { withFileTypes: true }
                )
                assert.deepStrictEqual(dirents[0].name, 'apples')
                assert.deepStrictEqual(dirents[1].name, 'link')
                assert.ok(dirents[0].isFile())
                assert.ok(dirents[1].isSymbolicLink())

                dirents = await patchedFs.promises.readdir(
                    path.join(fixturesDir, 'a'),
                    { withFileTypes: true }
                )
                assert.deepStrictEqual(dirents[0].name, 'apples')
                assert.deepStrictEqual(dirents[1].name, 'link')
                assert.ok(dirents[0].isFile())
                assert.ok(dirents[1].isSymbolicLink())

                // Assert the same with URL file references
                dirents = patchedFs.readdirSync(
                    new URL(`file://${path.join(fixturesDir, 'a')}`),
                    {
                        withFileTypes: true,
                    }
                )
                assert.deepStrictEqual(dirents[0].name, 'apples')
                assert.deepStrictEqual(dirents[1].name, 'link')
                assert.ok(dirents[0].isFile())
                assert.ok(dirents[1].isSymbolicLink())
            }
        )
    })

    await it('can readdir link dirents as files out of root', async () => {
        await withFixtures(
            {
                a: { apples: 'contents' },
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

                console.error('FOO')
                console.error(patchedFs.readdirSync)
                let dirents = patchedFs.readdirSync(
                    path.join(fixturesDir, 'a'),
                    {
                        withFileTypes: true,
                    }
                )
                console.error('BAR')
                console.log(dirents)
                assert.deepStrictEqual(dirents[0].name, 'apples')
                assert.deepStrictEqual(dirents[1].name, 'link')
                assert.ok(dirents[0].isFile())
                assert.ok(!dirents[1].isSymbolicLink())
                assert.ok(dirents[1].isFile())

                console.error('FOO')
                dirents = await util.promisify(patchedFs.readdir)(
                    path.join(fixturesDir, 'a'),
                    { withFileTypes: true }
                )
                console.error('BAR')
                assert.deepStrictEqual(dirents[0].name, 'apples')
                assert.deepStrictEqual(dirents[1].name, 'link')
                assert.ok(dirents[0].isFile())
                assert.ok(!dirents[1].isSymbolicLink())
                assert.ok(dirents[1].isFile())

                dirents = await patchedFs.promises.readdir(
                    path.join(fixturesDir, 'a'),
                    { withFileTypes: true }
                )
                assert.deepStrictEqual(dirents[0].name, 'apples')
                assert.deepStrictEqual(dirents[1].name, 'link')
                assert.ok(dirents[0].isFile())
                assert.ok(!dirents[1].isSymbolicLink())
                assert.ok(dirents[1].isFile())
            }
        )
    })

    await it('can readdir dirent in a sandbox', async () => {
        await withFixtures(
            {
                sandbox: {},
                execroot: { file: 'contents' },
            },
            async (fixturesDir) => {
                fixturesDir = fs.realpathSync(fixturesDir)

                // create symlink from execroot/link2 to execroot/file
                fs.symlinkSync(
                    path.join(fixturesDir, 'execroot', 'file'),
                    path.join(fixturesDir, 'execroot', 'link2')
                )
                // create symlink from execroot/link to execroot/link2
                fs.symlinkSync(
                    path.join(fixturesDir, 'execroot', 'link2'),
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
                fs.symlinkSync(
                    path.join(fixturesDir, 'execroot', 'link2'),
                    path.join(fixturesDir, 'sandbox', 'link2')
                )

                const patchedFs = Object.assign({}, fs)
                patchedFs.promises = Object.assign({}, fs.promises)

                patcher(patchedFs, [path.join(fixturesDir, 'sandbox')])

                let dirents = patchedFs.readdirSync(
                    path.join(fixturesDir, 'sandbox'),
                    {
                        withFileTypes: true,
                    }
                )
                assert.deepStrictEqual(dirents[0].name, 'file')
                assert.deepStrictEqual(dirents[1].name, 'link')
                assert.deepStrictEqual(dirents[2].name, 'link2')
                assert.ok(dirents[0].isFile())
                assert.ok(dirents[1].isSymbolicLink())
                assert.ok(dirents[2].isSymbolicLink())

                dirents = await util.promisify(patchedFs.readdir)(
                    path.join(fixturesDir, 'sandbox'),
                    { withFileTypes: true }
                )
                assert.deepStrictEqual(dirents[0].name, 'file')
                assert.deepStrictEqual(dirents[1].name, 'link')
                assert.deepStrictEqual(dirents[2].name, 'link2')
                assert.ok(dirents[0].isFile())
                assert.ok(dirents[1].isSymbolicLink())
                assert.ok(dirents[2].isSymbolicLink())

                dirents = await patchedFs.promises.readdir(
                    path.join(fixturesDir, 'sandbox'),
                    { withFileTypes: true }
                )
                assert.deepStrictEqual(dirents[0].name, 'file')
                assert.deepStrictEqual(dirents[1].name, 'link')
                assert.deepStrictEqual(dirents[2].name, 'link2')
                assert.ok(dirents[0].isFile())
                assert.ok(dirents[1].isSymbolicLink())
                assert.ok(dirents[2].isSymbolicLink())
            }
        )
    })

    await it('includes parent calls in stack traces', async function readdirStackTest1() {
        let err
        try {
            fs.readdirSync('/foo/bar' + Date.now())
        } catch (e) {
            err = e
        } finally {
            if (!err) assert.fail('readdirSync should fail on invalid path')
            if (!err.stack.includes('readdirStackTest1'))
                assert.fail(
                    `readdirSync error stack should contain calling method: ${err.stack}`
                )
        }
    })
})
