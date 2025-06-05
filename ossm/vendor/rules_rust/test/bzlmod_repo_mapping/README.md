Bzlmod allow modules to rename their dependencies and use the aliased names in runfiles lookups.
This test defines the following dependency tree: A -> B -> C. The test if whether a binary built in A
that calls code in B that has an `rlocation!` call to lookup a runfile from C is able to correctly use
B's repo_mapping. This is accomplished by having B use a custom `repo_name` for C.
