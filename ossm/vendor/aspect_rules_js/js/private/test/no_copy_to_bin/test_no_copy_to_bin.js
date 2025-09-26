console.log('\n\n\nThis is only a test.\n\n\n')
const secretToLifeTheUniverseAndEverything = require('./subpkg/42.js')
if (secretToLifeTheUniverseAndEverything != 42) {
    console.error('FAIL!')
    process.exit(1)
}
console.log(secretToLifeTheUniverseAndEverything)
