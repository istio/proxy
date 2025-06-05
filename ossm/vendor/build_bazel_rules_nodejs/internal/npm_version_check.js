#!/usr/bin/env node

// Fetch the version of this package from its package.json
const pkg = require('./package.json');
const pkgVersion = pkg.version ? pkg.version.split('-')[0] : '0.0.0';

// BUILD_BAZEL_RULES_NODEJS_VERSION is only set when within the bazel context
const rulesVersion = process.env['BUILD_BAZEL_RULES_NODEJS_VERSION'] || '0.0.0';

const getMajor = versionString => versionString ? versionString.split('.')[0] : '';

// Special cases when either version is 0.0.0.
// rulesVersion will be 0.0.0 when outside of bazel
// pkgVersion may be 0.0.0 for dev builds that are not stamped
if (rulesVersion !== '0.0.0' && pkgVersion !== '0.0.0' &&
    getMajor(pkgVersion) !== getMajor(rulesVersion)) {
  throw new Error(`Expected package major version to equal @build_bazel_rules_nodejs major version
    ${pkg.name} - ${pkgVersion}  
    @build_bazel_rules_nodejs - ${rulesVersion}
  See https://github.com/bazelbuild/rules_nodejs/wiki/Avoiding-version-skew`);
}
