const fs = require('fs')

/**
 * The status files are expected to look like
 * BUILD_SCM_HASH 83c699db39cfd74526cdf9bebb75aa6f122908bb
 * BUILD_SCM_LOCAL_CHANGES true
 * STABLE_BUILD_SCM_VERSION 6.0.0-beta.6+12.sha-83c699d.with-local-changes
 * BUILD_TIMESTAMP 1520021990506
 *
 * Parsing regex is created based on Bazel documentation describing the status file schema:
 *   The key names can be anything but they may only use upper case letters and underscores. The
 *   first space after the key name separates it from the value. The value is the rest of the line
 *   (including additional whitespace).
 */
function _parseStatusFile(statusFilePath) {
    const results = {}
    const statusFile = fs.readFileSync(statusFilePath, { encoding: 'utf-8' })
    for (const match of `\n${statusFile}`.matchAll(/^([^\s]+)\s+(.*)/gm)) {
        // Lines which go unmatched define an index value of `0` and should be skipped.
        if (match.index === 0) {
            continue
        }
        results[match[1]] = match[2]
    }
    return results
}

function _unquoteArgs(s) {
    return s.replace(/^'(.*)'$/, '$1')
}

function _replaceAll(value, token, replacement) {
    // String.prototype.replaceAll was only added in Node.js 5; polyfill
    // if it is not available
    if (value.replaceAll) {
        return value.replaceAll(token, replacement)
    } else {
        while (value.indexOf(token) != -1) {
            value = value.replace(token, replacement)
        }
        return value
    }
}

function main(args) {
    args = fs
        .readFileSync(args[0], { encoding: 'utf-8' })
        .split('\n')
        .map(_unquoteArgs)
    const [
        template,
        out,
        volatileStatusFile,
        stableStatusFile,
        substitutionsJson,
        isExecutable,
    ] = args

    const substitutions = JSON.parse(substitutionsJson)

    const statuses = {
        ..._parseStatusFile(volatileStatusFile),
        ..._parseStatusFile(stableStatusFile),
    }

    const statusSubstitutions = []
    for (const key of Object.keys(statuses)) {
        statusSubstitutions.push([`{{${key}}}`, statuses[key]])
    }

    for (const key of Object.keys(substitutions)) {
        let value = substitutions[key]
        statusSubstitutions.forEach(([token, replacement]) => {
            value = _replaceAll(value, token, replacement)
        })
        substitutions[key] = value
    }

    let content = fs.readFileSync(template, { encoding: 'utf-8' })
    for (const key of Object.keys(substitutions)) {
        content = _replaceAll(content, key, substitutions[key])
    }
    const mode = isExecutable ? 0o777 : 0x666
    fs.writeFileSync(out, content, { mode })
}

if (require.main === module) {
    process.exitCode = main(process.argv.slice(2))
}
