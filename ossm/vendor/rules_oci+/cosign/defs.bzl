"Public API"

load("//cosign/private:attest.bzl", _cosign_attest = "cosign_attest")
load("//cosign/private:sign.bzl", _cosign_sign = "cosign_sign")

cosign_sign = _cosign_sign
cosign_attest = _cosign_attest
