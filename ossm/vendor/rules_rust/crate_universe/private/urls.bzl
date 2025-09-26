"""A file containing urls and associated sha256 values for cargo-bazel binaries

This file is auto-generated for each release to match the urls and sha256s of
the binaries produced for it.
"""

# Example:
# {
#     "x86_64-unknown-linux-gnu": "https://domain.com/downloads/cargo-bazel-x86_64-unknown-linux-gnu",
#     "x86_64-apple-darwin": "https://domain.com/downloads/cargo-bazel-x86_64-apple-darwin",
#     "x86_64-pc-windows-msvc": "https://domain.com/downloads/cargo-bazel-x86_64-pc-windows-msvc",
# }
CARGO_BAZEL_URLS = {
  "aarch64-apple-darwin": "https://github.com/bazelbuild/rules_rust/releases/download/0.56.0/cargo-bazel-aarch64-apple-darwin",
  "aarch64-pc-windows-msvc": "https://github.com/bazelbuild/rules_rust/releases/download/0.56.0/cargo-bazel-aarch64-pc-windows-msvc.exe",
  "aarch64-unknown-linux-gnu": "https://github.com/bazelbuild/rules_rust/releases/download/0.56.0/cargo-bazel-aarch64-unknown-linux-gnu",
  "x86_64-apple-darwin": "https://github.com/bazelbuild/rules_rust/releases/download/0.56.0/cargo-bazel-x86_64-apple-darwin",
  "x86_64-pc-windows-gnu": "https://github.com/bazelbuild/rules_rust/releases/download/0.56.0/cargo-bazel-x86_64-pc-windows-gnu.exe",
  "x86_64-pc-windows-msvc": "https://github.com/bazelbuild/rules_rust/releases/download/0.56.0/cargo-bazel-x86_64-pc-windows-msvc.exe",
  "x86_64-unknown-linux-gnu": "https://github.com/bazelbuild/rules_rust/releases/download/0.56.0/cargo-bazel-x86_64-unknown-linux-gnu",
  "x86_64-unknown-linux-musl": "https://github.com/bazelbuild/rules_rust/releases/download/0.56.0/cargo-bazel-x86_64-unknown-linux-musl"
}

# Example:
# {
#     "x86_64-unknown-linux-gnu": "1d687fcc860dc8a1aa6198e531f0aee0637ed506d6a412fe2b9884ff5b2b17c0",
#     "x86_64-apple-darwin": "0363e450125002f581d29cf632cc876225d738cfa433afa85ca557afb671eafa",
#     "x86_64-pc-windows-msvc": "f5647261d989f63dafb2c3cb8e131b225338a790386c06cf7112e43dd9805882",
# }
CARGO_BAZEL_SHA256S = {
  "aarch64-apple-darwin": "5143e347ec45e0fcc249f1de44f0dce227062579ad364a0b269b39f0ce49f1d8",
  "aarch64-pc-windows-msvc": "ac9ee9982f3083687e3647f7dd0a01b7ef128151511e333ad2d7ae37199071ec",
  "aarch64-unknown-linux-gnu": "f3a35b137221d4627ba0ed62f775c4d3cbb45b6db839ae47f2ebfd48abe44a91",
  "x86_64-apple-darwin": "9e8a92674771982f68031d0cc516ba08b34dd736c4f818d51349547f2136d012",
  "x86_64-pc-windows-gnu": "9bb64533156419e7c551f20f7cd316d031b6869f72b735e89d150430de9982ea",
  "x86_64-pc-windows-msvc": "5c62a1c42b71af6c2765dc5f9518eba22ed28276a65f7accf5a2bca0c5cfaf74",
  "x86_64-unknown-linux-gnu": "53418ae5457040e84009f7c69b070e1f12f10a9a3a3ef4f5d0829c66e5438ba1",
  "x86_64-unknown-linux-musl": "a84ecc2d0b4365e5a43c0a257b1be1a35ffa62bbcdb5d680741b142922654643"
}

# Example:
# Label("//crate_universe:cargo_bazel_bin")
CARGO_BAZEL_LABEL = Label("//crate_universe:cargo_bazel_bin")
