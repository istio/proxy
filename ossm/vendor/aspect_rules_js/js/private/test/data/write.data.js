// write.data.js out.json in.json

const fs = require('fs')

fs.writeFileSync(process.argv[2], fs.readFileSync(process.argv[3], 'utf8'))
