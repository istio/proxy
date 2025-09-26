const assert = require('assert')

const args = process.argv.slice(2)
assert.equal(args.length, 2)
assert.equal(args[0], '--arg1')
assert.equal(args[1], '--arg2')

assert.equal(process.env['ENV1'], 'foo')
assert.equal(process.env['ENV2'], 'bar')
