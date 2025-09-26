# This is a renovate-friendly source of Docker images.
FROM davidanson/markdownlint-cli2:v0.18.1@sha256:173cb697a255a8a985f2c6a83b4f7a8b3c98f4fb382c71c45f1c52e4d4fed63a AS markdownlint
FROM lycheeverse/lychee:sha-3a09227-alpine@sha256:5853bd7c283663a1200dbb15924a5047f8d4c50adfa7a4c212a94f04bbac831c AS lychee
