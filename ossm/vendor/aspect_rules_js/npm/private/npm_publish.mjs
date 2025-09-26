import { spawnSync } from 'node:child_process'

const restArgs = process.argv.slice(2)

const spawn = spawnSync('npm', ['publish', ...restArgs], {
    stdio: 'inherit',
})

process.exit(spawn.status)
