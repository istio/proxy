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
const path = require('path')
const escapeFunction =
    require('../../node-patches_legacy/src/fs').escapeFunction
const isSubPath = require('../../node-patches_legacy/src/fs').isSubPath

// We don't want to bring jest into this repo so we just fake the describe and it functions here
async function describe(_, fn) {
    await fn()
}
async function it(_, fn) {
    await fn()
}

describe('escape function', () => {
    it('isSubPath is correct', () => {
        assert.ok(!isSubPath('/a/b', '/a'))
        assert.ok(!isSubPath('/a/b', '/a/c/b'))
        assert.ok(isSubPath('/a/b', '/a/b'))
        assert.ok(isSubPath('/a/b', '/a/b/c/d'))
    })

    it('isEscape is correct', () => {
        const roots = ['/a/b', '/a/b/g/1', '/a/b/g/a/2', '/a/b/g/a/3']
        const { isEscape } = escapeFunction(roots)

        assert.ok(isEscape('/a/b/l', '/a/c/boop'))
        assert.ok(isEscape('/a/b', '/a/c/boop'))
        assert.ok(isEscape('/a/b', '/a'))
        assert.ok(!isEscape('/a/c', '/a/c/boop'))
        assert.ok(!isEscape('/a/b/l', '/a/b/f'))

        assert.ok(isEscape('/a/b/g/1', '/some/path'))
        assert.ok(isEscape('/a/b/g/1/foo', '/some/path'))
        assert.ok(isEscape('/a/b/g/h', '/some/path'))
        assert.ok(isEscape('/a/b/g/h/i', '/some/path'))
        assert.ok(isEscape('/a/b/g/a/2', '/some/path'))
        assert.ok(isEscape('/a/b/g/a/2/foo', '/some/path'))
        assert.ok(isEscape('/a/b/g/a/3', '/some/path'))
        assert.ok(isEscape('/a/b/g/a/3/foo', '/some/path'))
        assert.ok(isEscape('/a/b/g/a/h', '/some/path'))
        assert.ok(isEscape('/a/b/g/a/h/i', '/some/path'))

        assert.ok(isEscape('/a/b/g/1', '/a/b'))
        assert.ok(isEscape('/a/b/g/1/foo', '/a/b'))
        assert.ok(!isEscape('/a/b/g/h', '/a/b'))
        assert.ok(!isEscape('/a/b/g/h/i', '/a/b'))
        assert.ok(isEscape('/a/b/g/a/2', '/a/b'))
        assert.ok(isEscape('/a/b/g/a/2/foo', '/a/b'))
        assert.ok(isEscape('/a/b/g/a/3', '/a/b'))
        assert.ok(isEscape('/a/b/g/a/3/foo', '/a/b'))
        assert.ok(!isEscape('/a/b/g/a/h', '/a/b'))
        assert.ok(!isEscape('/a/b/g/a/h/i', '/a/b'))

        assert.ok(isEscape('/a/b/g/1', '/a/b/c'))
        assert.ok(isEscape('/a/b/g/1/foo', '/a/b/c'))
        assert.ok(!isEscape('/a/b/g/h', '/a/b/c'))
        assert.ok(!isEscape('/a/b/g/h/i', '/a/b/c'))
        assert.ok(isEscape('/a/b/g/a/2', '/a/b/c'))
        assert.ok(isEscape('/a/b/g/a/2/foo', '/a/b/c'))
        assert.ok(isEscape('/a/b/g/a/3', '/a/b/c'))
        assert.ok(isEscape('/a/b/g/a/3/foo', '/a/b/c'))
        assert.ok(!isEscape('/a/b/g/a/h', '/a/b/c'))
        assert.ok(!isEscape('/a/b/g/a/h/i', '/a/b/c'))
    })

    it('isEscape handles relative paths', () => {
        const roots = ['./a/b', './a/b/g/1', './a/b/g/a/2', './a/b/g/a/3']
        const { isEscape } = escapeFunction(roots)

        assert.ok(isEscape('./a/b/l', path.resolve('./a/c/boop')))
        assert.ok(isEscape('./a/b', path.resolve('./a/c/boop')))
        assert.ok(isEscape('./a/b', path.resolve('./a')))
        assert.ok(!isEscape('./a/c', path.resolve('./a/c/boop')))
        assert.ok(!isEscape('./a/b/l', path.resolve('./a/b/f')))

        assert.ok(isEscape('./a/b/g/1', path.resolve('./some/path')))
        assert.ok(isEscape('./a/b/g/1/foo', path.resolve('./some/path')))
        assert.ok(isEscape('./a/b/g/h', path.resolve('./some/path')))
        assert.ok(isEscape('./a/b/g/h/i', path.resolve('./some/path')))
        assert.ok(isEscape('./a/b/g/a/2', path.resolve('./some/path')))
        assert.ok(isEscape('./a/b/g/a/2/foo', path.resolve('./some/path')))
        assert.ok(isEscape('./a/b/g/a/3', path.resolve('./some/path')))
        assert.ok(isEscape('./a/b/g/a/3/foo', path.resolve('./some/path')))
        assert.ok(isEscape('./a/b/g/a/h', path.resolve('./some/path')))
        assert.ok(isEscape('./a/b/g/a/h/i', path.resolve('./some/path')))

        assert.ok(isEscape('./a/b/g/1', path.resolve('./a/b')))
        assert.ok(isEscape('./a/b/g/1/foo', path.resolve('./a/b')))
        assert.ok(!isEscape('./a/b/g/h', path.resolve('./a/b')))
        assert.ok(!isEscape('./a/b/g/h/i', path.resolve('./a/b')))
        assert.ok(isEscape('./a/b/g/a/2', path.resolve('./a/b')))
        assert.ok(isEscape('./a/b/g/a/2/foo', path.resolve('./a/b')))
        assert.ok(isEscape('./a/b/g/a/3', path.resolve('./a/b')))
        assert.ok(isEscape('./a/b/g/a/3/foo', path.resolve('./a/b')))
        assert.ok(!isEscape('./a/b/g/a/h', path.resolve('./a/b')))
        assert.ok(!isEscape('./a/b/g/a/h/i', path.resolve('./a/b')))

        assert.ok(isEscape('./a/b/g/1', path.resolve('./a/b/c')))
        assert.ok(isEscape('./a/b/g/1/foo', path.resolve('./a/b/c')))
        assert.ok(!isEscape('./a/b/g/h', path.resolve('./a/b/c')))
        assert.ok(!isEscape('./a/b/g/h/i', path.resolve('./a/b/c')))
        assert.ok(isEscape('./a/b/g/a/2', path.resolve('./a/b/c')))
        assert.ok(isEscape('./a/b/g/a/2/foo', path.resolve('./a/b/c')))
        assert.ok(isEscape('./a/b/g/a/3', path.resolve('./a/b/c')))
        assert.ok(isEscape('./a/b/g/a/3/foo', path.resolve('./a/b/c')))
        assert.ok(!isEscape('./a/b/g/a/h', path.resolve('./a/b/c')))
        assert.ok(!isEscape('./a/b/g/a/h/i', path.resolve('./a/b/c')))
    })
})
