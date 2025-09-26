const dataPath = process.argv[2] || './data.json'

const { answer } = require(dataPath)

if (answer !== 42) {
    throw new Error(`The answer (${answer}) is not 42!`)
}
