import {
    isNodeModulePath,
    is1pVirtualStoreDep,
    friendlyFileSize,
} from '../../js_run_devserver.mjs'

// isNodeModulePath
const isNodeModulePath_true = [
    '/private/var/some/path/node_modules/@babel/core',
    '/private/var/some/path/node_modules/lodash',
]
for (const p of isNodeModulePath_true) {
    if (!isNodeModulePath(p)) {
        console.error(`ERROR: expected ${p} to be a node_modules path`)
        process.exit(1)
    }
}
const isNodeModulePath_false = ['/private/var/some/path/some-file.js']
for (const p of isNodeModulePath_false) {
    if (isNodeModulePath(p)) {
        console.error(`ERROR: expected ${p} to not be a node_modules path`)
        process.exit(1)
    }
}

// is1pVirtualStoreDep
const is1pVirtualStoreDep_true = [
    'some/path/node_modules/.aspect_rules_js/@mycorp+pkg@0.0.0/node_modules/@mycorp/pkg',
    'some/path/node_modules/.aspect_rules_js/mycorp-pkg@0.0.0/node_modules/mycorp-pkg',
]
for (const p of is1pVirtualStoreDep_true) {
    if (!is1pVirtualStoreDep(p)) {
        console.error(`ERROR: expected ${p} to be a 1p virtual store dep`)
        process.exit(1)
    }
}
const is1pVirtualStoreDep_false = [
    'some/path/node_modules/.aspect_rules_js/@mycorp+pkg@0.0.0/node_modules/mycorp/pkg',
    'some/path/node_modules/.aspect_rules_js/@mycorp+pkg0.0.0/node_modules/@mycorp/pkg',
    'some/path/node_modules/.aspect_rules_js/mycorp+pkg@0.0.0/node_modules/@mycorp/pkg',
    'some/path/node_modules/.aspect_rules_js/mycorp-pkg0.0.0/node_modules/mycorp-pkg',
    'some/path/node_modules/.aspect_rules_js/@mycorp+pkg@0.0.0/node_modules/acorn',
    'some/path/node_modules/.aspect_rules_js/mycorp-pkg@0.0.0/node_modules/acorn',
    'some/path/node_modules/.aspect_rules_js/@babel+runtime@7.21.0/node_modules/@babel/runtime',
    'some/path/node_modules/.aspect_rules_js/@babel+runtime@7.21.0/node_modules/acorn',
    'some/path/node_modules/.aspect_rules_js/eval@0.1.6/node_modules/eval',
    'some/path/node_modules/.aspect_rules_js/eval@0.1.6/node_modules/acorn',
]
for (const p of is1pVirtualStoreDep_false) {
    if (is1pVirtualStoreDep(p)) {
        console.error(`ERROR: expected ${p} to not be a 1p virtual store dep`)
        process.exit(1)
    }
}

// friendlyFileSize
const friendlyFileSize_cases = new Map()
friendlyFileSize_cases.set(0, '0 B')
friendlyFileSize_cases.set(1, '1 B')
friendlyFileSize_cases.set(100, '100 B')
friendlyFileSize_cases.set(1023, '1023 B')
friendlyFileSize_cases.set(1024, '1.0 KiB')
friendlyFileSize_cases.set(1300, '1.3 KiB')
friendlyFileSize_cases.set(1024 * 1024, '1.0 MiB')
friendlyFileSize_cases.set(1024 * 1024 * 1024, '1.0 GiB')
friendlyFileSize_cases.set(1024 * 1024 * 1024 * 1024, '1.0 TiB')
friendlyFileSize_cases.set(1024 * 1024 * 1024 * 1024 * 1024, '1.0 PiB')
friendlyFileSize_cases.set(
    1024 * 1024 * 1024 * 1024 * 1024 * 1024,
    '1024.0 PiB'
)
for (const [k, v] of friendlyFileSize_cases) {
    const a = friendlyFileSize(k)
    if (a !== v) {
        console.error(
            `Expected friendlyFileSize(${k}) to be '${v}' but got '${a}'`
        )
        process.exit(1)
    }
}
