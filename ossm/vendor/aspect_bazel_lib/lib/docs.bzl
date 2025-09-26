"Public API for docs helpers"

load(
    "//lib/private:docs.bzl",
    _stardoc_with_diff_test = "stardoc_with_diff_test",
    _update_docs = "update_docs",
)

stardoc_with_diff_test = _stardoc_with_diff_test
update_docs = _update_docs
