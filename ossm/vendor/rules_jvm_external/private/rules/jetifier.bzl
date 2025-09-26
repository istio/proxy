# Some rulesets (eg. `contrib_rules_jvm`) have "frozen" their `rules_jvm_external`
# dependencies to avoid needing consumers of those rulesets from needing to run
# a resolution. Provide a place-holder `jetifier.bzl` file so that those rulesets
# don't need to jump to the latest `rules_jvm_external` immediately.

def jetify_aar_import(**kwargs):
    fail("This functionality is no longer supported")

def jetify_jvm_import(**kwargs):
    fail("This functionality is no longer supported")
