'use strict'

var semver = require('semver')

var makeMax2 = function (op) {
    return function (v1, v2) {
        return semver[op](v2.clean, v1.clean) ? v2 : v1
    }
}

var makeMax = function (op) {
    return function () {
        return [].filter
            .call(arguments, function (version) {
                return semver.valid(version, true)
            })
            .map(function (version) {
                return {
                    original: version,
                    clean: semver.clean(version),
                }
            })
            .reduce(makeMax2(op)).original
    }
}

module.exports = (function (max) {
    max.gt = max
    ;['gte', 'lt', 'lte'].forEach(function (op) {
        max[op] = makeMax(op)
    })

    return max
})(makeMax('gt'))
