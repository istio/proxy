#!/usr/bin/env bash

set -euo pipefail

# Replace the load statements
find ./ -type f -name BUILD* -exec sed -i '' -e 's/gmaven_rules/rules_jvm_external/g' {} \;

# Remove the explicit packaging type from the coordinates
find ./ -type f -name BUILD* -exec sed -i '' -e '/gmaven_artifact(/s/:aar//g; /gmaven_artifact(/s/:jar//g;' {} \;

# Replace all occurrences of gmaven_artifact with artifact
find ./ -type f -name BUILD* -exec sed -i '' -e 's/gmaven_artifact/artifact/g' {} \;

echo ""
echo "Paste this into your WORKSPACE file:"
echo "-------------------------"
echo ""

cat <<-EOF
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

RULES_JVM_EXTERNAL_TAG = "1.2"
RULES_JVM_EXTERNAL_SHA = "e5c68b87f750309a79f59c2b69ead5c3221ffa54ff9496306937bfa1c9c8c86b"

http_archive(
    name = "rules_jvm_external",
    strip_prefix = "rules_jvm_external-%s" % RULES_JVM_EXTERNAL_TAG,
    sha256 = RULES_JVM_EXTERNAL_SHA,
    url = "https://github.com/bazelbuild/rules_jvm_external/archive/%s.zip" % RULES_JVM_EXTERNAL_TAG,
)

load("@rules_jvm_external//:defs.bzl", "maven_install")

maven_install(
    name = "maven",
    artifacts = [
EOF

# Grep for all Maven artifact IDs and uniquify them.
# Steps
# 1. Get the list of maven_artifact declarations
# 2. Remove the explicit packaging type
# 3. Get the string to the left of the right bracket
# 4. Get the string to the right of the left bracket
# 5. Sort and de-duplicate
# 6. Format for WORKSPACE
find ./ -type f -name BUILD* -exec grep "artifact(.*)" {} \; \
  | sed -e "s/:aar//; s/:jar//" \
  | cut -d')' -f1 \
  | cut -d'(' -f2 \
  | sort \
  | uniq \
  | sed 's/^/        /; s/$/,/;'

cat <<-EOF
    ],
    repositories = [
        "https://maven.google.com",
        "https://repo1.maven.org/maven2",
    ],
)
EOF

echo ""
echo "-------------------------"
echo ""
echo "Note that gmaven_rules only handled artifacts hosted on Google Maven, so you may need to add more repositories into `maven_install.repositories`."
