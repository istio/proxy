"""Rules for extracting version information from bzlmod files."""

load("@aspect_bazel_lib//lib:jq.bzl", "jq")

_DEFAULT_MODULE_FILTER = """
gsub("\\n"; " ")
| [scan("bazel_dep\\\\([^)]*name\\\\s*=\\\\s*\\"([^\\"]+)\\"[^)]*version\\\\s*=\\\\s*\\"([^\\"]+)\\"[^)]*\\\\)")]
| map({name: .[0], version: .[1]})
| map({(.name): {minimum_version: .version}})
| add // {}
"""

_DEFAULT_LOCK_FILTER = """
def version_key:
  if contains("-") then
    (split("-")
     | {release: (.[0]
                  | split(".")
                  | map(if test("^[0-9]+$") then [0, tonumber] else [1, .] end)),
        has_prerelease: true,
        prerelease: (.[1:]
                     | join("-")
                     | split(".")
                     | map(if test("^[0-9]+$") then [0, tonumber] else [1, .] end))})
  else
    {release: (split(".")
               | map(if test("^[0-9]+$") then [0, tonumber] else [1, .] end)),
     has_prerelease: false,
     prerelease: []}
  end
  | [.release, (if .has_prerelease then 0 else 1 end), .prerelease];

.registryFileHashes // {}
| keys
| map(select(test("^https://[^/]+/modules/[^/]+/[^/]+/MODULE.bazel$")))
| map(capture("^(?<registry>https://[^/]+)/modules/(?<name>[^/]+)/(?<version>[^/]+)/MODULE.bazel$"))
| group_by(.name)
| map({name: .[0].name,
       registry: .[0].registry,
       version: (map({version: .version, key: (.version | version_key)})
                 | sort_by(.key)
                 | last
                 | .version)})
| map({(.name): {version: .version, registry: .registry}})
| add // {}
"""

_MERGE_FILTER = """
.[0] as $min
| .[1] as $res
| $min
| keys
| map(. as $key
      | {($key): (($min[$key] // {}) * ($res[$key] // {}))})
| add // {}
"""

def module_versions(
        name,
        module_bazel = None,
        module_lock = None,
        module_filter = None,
        lock_filter = None,
        visibility = None):
    """Extracts version information from MODULE.bazel and MODULE.bazel.lock files.

    This macro uses jq to parse bzlmod dependency declarations and produce a JSON file
    mapping module names to their minimum (declared) and resolved versions.

    The resolved version is determined by taking the highest semantic version from
    all versions in the lockfile for each module, as the lockfile contains all versions
    that were considered during resolution.

    Example usage:

    ```starlark
    load("@envoy_toolshed//bazel/version:utils.bzl", "module_versions")

    module_versions(
        name = "versions",
        module_bazel = "//:MODULE.bazel",
        module_lock = "//:MODULE.bazel.lock",
    )
    ```

    This produces a `versions.json` file with structure:

    ```json
    {
      "aspect_bazel_lib": {
        "minimum_version": "2.22.0",
        "version": "2.22.0",
        "registry": "https://bcr.bazel.build"
      },
      "rules_python": {
        "minimum_version": "1.7.0",
        "version": "1.7.0",
        "registry": "https://bcr.bazel.build"
      }
    }
    ```

    Args:
        name: Name of the target. Output will be <name>.json
        module_bazel: Label for MODULE.bazel file (default: //:MODULE.bazel)
        module_lock: Label for MODULE.bazel.lock file (default: //:MODULE.bazel.lock)
        module_filter: Optional custom jq filter for parsing MODULE.bazel
        lock_filter: Optional custom jq filter for parsing MODULE.bazel.lock
        visibility: Visibility of the target
    """
    module_bazel = module_bazel or "//:MODULE.bazel"
    module_lock = module_lock or "//:MODULE.bazel.lock"
    module_filter = module_filter or _DEFAULT_MODULE_FILTER
    lock_filter = lock_filter or _DEFAULT_LOCK_FILTER

    jq(
        name = "%s_minimum" % name,
        srcs = [module_bazel],
        out = "%s_minimum.json" % name,
        filter = module_filter,
        args = ["-Rs"],
    )

    jq(
        name = "%s_resolved" % name,
        srcs = [module_lock],
        out = "%s_resolved.json" % name,
        filter = lock_filter,
    )

    jq(
        name = name,
        srcs = [
            ":%s_minimum" % name,
            ":%s_resolved" % name,
        ],
        out = "%s.json" % name,
        filter = _MERGE_FILTER,
        args = ["--slurp"],
        visibility = visibility,
    )
