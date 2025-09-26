// NB: on windows thanks to legacy 8-character path segments it might be like
// c:/b/ojvxx6nx/execroot/build_~1/bazel-~1/x64_wi~1/bin/internal/npm_in~1/test
export const BAZEL_OUT_REGEX = /(\/bazel-out\/|\/bazel-~1\/x64_wi~1\/)/;

// The runfiles root symlink under which the repository mapping can be found.
// https://cs.opensource.google/bazel/bazel/+/1b073ac0a719a09c9b2d1a52680517ab22dc971e:src/main/java/com/google/devtools/build/lib/analysis/Runfiles.java;l=424
export const REPO_MAPPING_RLOCATION = '_repo_mapping';
