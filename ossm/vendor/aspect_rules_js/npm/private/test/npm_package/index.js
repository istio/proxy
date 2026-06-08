// Import two versions (via aliases) of the same package, see https://github.com/aspect-build/rules_js/issues/1110
const chalk = require('chalk')
const chalkAlt = require('chalk-alt')

console.log(chalk.italic.green('Answer:'), chalkAlt.bold.red('42'))
