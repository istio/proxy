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

const patcher = require('../../node-patches/src/fs').patcher

// We don't want to bring jest into this repo so we just fake the describe and it functions here
async function describe(_, fn) {
    await fn()
}
async function it(_, fn) {
    await fn()
}

describe('testing lstat', async () => {
    await it('can lstat symlink in root', async () => {
        await withFixtures(
            {
                a: {},
                b: { file: 'contents' },
            },
            async (fixturesDir) => {
                fixturesDir = fs.realpathSync(fixturesDir)
                // create symlink from a/link to b/file
                fs.symlinkSync(
                    path.join(fixturesDir, 'b', 'file'),
                    path.join(fixturesDir, 'a', 'link')
                )

                const patchedFs = Object.assign({}, fs)
                patchedFs.promises = Object.assign({}, fs.promises)
                patcher(patchedFs, [path.join(fixturesDir)])

                const linkPath = path.join(fixturesDir, 'a', 'link')
                assert.ok(
                    patchedFs.lstatSync(linkPath).isSymbolicLink(),
                    'lstatSync should find symbolic link if link is the root'
                )

                assert.ok(
                    (
                        await util.promisify(patchedFs.lstat)(linkPath)
                    ).isSymbolicLink(),
                    'lstat should find symbolic link if link is the root'
                )

                assert.ok(
                    (await patchedFs.promises.lstat(linkPath)).isSymbolicLink(),
                    'promises.lstat should find symbolic link if link is the root'
                )
            }
        )
    })

    await it('can lstatSync throwIfNoEntry:false', async () => {
        await withFixtures(
            {
                a: { g: {} },
                b: { file: 'contents' },
            },
            async (fixturesDir) => {
                fixturesDir = fs.realpathSync(fixturesDir)

                const patchedFs = Object.assign({}, fs)
                patchedFs.promises = Object.assign({}, fs.promises)
                patcher(patchedFs, [
                    path.join(fixturesDir),
                    path.join(fixturesDir, 'a', 'g'),
                ])

                assert.equal(
                    undefined,
                    patchedFs.lstatSync(
                        path.join(fixturesDir, 'doesnt-exist'),
                        { throwIfNoEntry: false }
                    )
                )
            }
        )
    })

    await it('can lstat symlink in guard is file', async () => {
        await withFixtures(
            {
                a: { g: {} },
                b: { file: 'contents' },
            },
            async (fixturesDir) => {
                fixturesDir = fs.realpathSync(fixturesDir)
                // create symlink from a/g/link to b/file
                fs.symlinkSync(
                    path.join(fixturesDir, 'b', 'file'),
                    path.join(fixturesDir, 'a', 'g', 'link')
                )

                const patchedFs = Object.assign({}, fs)
                patchedFs.promises = Object.assign({}, fs.promises)
                patcher(patchedFs, [
                    path.join(fixturesDir),
                    path.join(fixturesDir, 'a', 'g'),
                ])

                const linkPath = path.join(fixturesDir, 'a', 'g', 'link')
                assert.ok(
                    patchedFs.lstatSync(linkPath).isFile(),
                    'lstatSync should find file if link is in guard'
                )

                assert.ok(
                    (await util.promisify(patchedFs.lstat)(linkPath)).isFile(),
                    'lstat should find file if link is in guard'
                )

                assert.ok(
                    (await patchedFs.promises.lstat(linkPath)).isFile(),
                    'promises.lstat should find file if link is in guard'
                )
            }
        )
    })

    await it('lstat of symlink out of root is file.', async () => {
        await withFixtures(
            {
                a: {},
                b: { file: 'contents' },
            },
            async (fixturesDir) => {
                fixturesDir = fs.realpathSync(fixturesDir)
                // create symlink from a/link to b/file
                fs.symlinkSync(
                    path.join(fixturesDir, 'b', 'file'),
                    path.join(fixturesDir, 'a', 'link')
                )

                const patchedFs = Object.assign({}, fs)
                patchedFs.promises = Object.assign({}, fs.promises)
                patcher(patchedFs, [path.join(fixturesDir, 'a')])

                const linkPath = path.join(fixturesDir, 'a', 'link')

                assert.ok(
                    patchedFs.lstatSync(linkPath).isFile(),
                    'lstatSync should find file it file link is out of root'
                )

                assert.ok(
                    (await util.promisify(patchedFs.lstat)(linkPath)).isFile(),
                    'lstat should find file it file link is out of root'
                )

                assert.ok(
                    (await patchedFs.promises.lstat(linkPath)).isFile(),
                    'promises.lstat should find file it file link is out of root'
                )

                let brokenLinkPath = path.join(fixturesDir, 'a', 'broken-link')
                fs.symlinkSync(
                    path.join(fixturesDir, 'doesnt-exist'),
                    brokenLinkPath
                )

                assert.ok(
                    patchedFs.lstatSync(brokenLinkPath).isSymbolicLink(),
                    'if a symlink is broken but is escaping return it as a link.'
                )
                assert.ok(
                    (
                        await util.promisify(patchedFs.lstat)(brokenLinkPath)
                    ).isSymbolicLink(),
                    'if a symlink is broken but is escaping return it as a link.'
                )
                assert.ok(
                    (
                        await patchedFs.promises.lstat(brokenLinkPath)
                    ).isSymbolicLink(),
                    'if a symlink is broken but is escaping return it as a link.'
                )

                brokenLinkPath = path.join(fixturesDir, 'a', 'broken-link2')
                fs.symlinkSync(
                    path.join(fixturesDir, 'a', 'doesnt-exist'),
                    brokenLinkPath
                )

                stat = await patchedFs.promises.lstat(brokenLinkPath)
                assert.ok(
                    stat.isSymbolicLink(),
                    'if a symlink is broken but not escaping return it as a link.'
                )
            }
        )
    })

    await it('not in root, symlinks do what they would have before.', async () => {
        await withFixtures(
            {
                a: {},
                b: { file: 'contents' },
            },
            async (fixturesDir) => {
                fixturesDir = fs.realpathSync(fixturesDir)
                // create symlink from a/link to b/file
                fs.symlinkSync(
                    path.join(fixturesDir, 'b', 'file'),
                    path.join(fixturesDir, 'b', 'link')
                )

                const patchedFs = Object.assign({}, fs)
                patchedFs.promises = Object.assign({}, fs.promises)
                patcher(patchedFs, [path.join(fixturesDir, 'a')])

                const linkPath = path.join(fixturesDir, 'b', 'link')

                assert.ok(
                    patchedFs.lstatSync(linkPath).isSymbolicLink(),
                    'lstatSync should find symbolic link if link is out of the root'
                )

                assert.ok(
                    patchedFs
                        .lstatSync(new URL(`file://${linkPath}`))
                        .isSymbolicLink(),
                    'lstatSync should find symbolic link if link is out of the root'
                )

                const stat = await util.promisify(patchedFs.lstat)(linkPath)
                assert.ok(
                    stat.isSymbolicLink(),
                    'lstat should find symbolic link if link is outside'
                )

                assert.ok(
                    (await patchedFs.promises.lstat(linkPath)).isSymbolicLink(),
                    'promises.lstat should find symbolic link if link is outside'
                )
            }
        )
    })

    await it('includes parent calls in stack traces', async function lstatStackTest1() {
        let err
        try {
            fs.lstatSync(null)
        } catch (e) {
            err = e
        } finally {
            if (!err) assert.fail('lstat should fail on invalid path')
            if (!err.stack.includes('lstatStackTest1'))
                assert.fail(
                    `lstat error stack should contain calling method: ${err.stack}`
                )
        }
    })
})
