"""Utility functions for encoding and decoding strings with base64.

See https://en.wikipedia.org/wiki/Base64.
"""

load("//lib/private:base64.bzl", _decode = "decode", _encode = "encode")

base64 = struct(
    decode = _decode,
    encode = _encode,
)
