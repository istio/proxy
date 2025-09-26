
VERSIONS = {
    "go": "1.23.1",
    "python": "3.12",

    "io_bazel_rules_go": {
        "type": "github_archive",
        "repo": "bazelbuild/rules_go",
        "version": "0.53.0",
        "sha256": "b78f77458e77162f45b4564d6b20b6f92f56431ed59eaaab09e7819d1d850313",
        "url": "https://github.com/bazelbuild/rules_go/releases/download/v{version}/rules_go-v{version}.zip",
    },

    "rules_pkg": {
        "type": "github_archive",
        "repo": "bazelbuild/rules_pkg",
        "version": "1.1.0",
        "sha256": "b7215c636f22c1849f1c3142c72f4b954bb12bb8dcf3cbe229ae6e69cc6479db",
        "url": "https://github.com/bazelbuild/rules_pkg/releases/download/{version}/rules_pkg-{version}.tar.gz",
    },

    "rules_python": {
        "type": "github_archive",
        "repo": "bazelbuild/rules_python",
        "version": "1.4.1",
        "sha256": "9f9f3b300a9264e4c77999312ce663be5dee9a56e361a1f6fe7ec60e1beef9a3",
        "url": "https://github.com/{repo}/releases/download/{version}/{name}-{version}.tar.gz",
        "strip_prefix": "{name}-{version}",
    },

}
