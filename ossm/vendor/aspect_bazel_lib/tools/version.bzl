"version information. replaced with stamped info with each release"

# This is automagically replace by git during git archive using `git export-subst`
# See https://git-scm.com/docs/git-archive#Documentation/git-archive.txt-export-subst
_VERSION_PRIVATE = "v2.14.0"

VERSION = "0.0.0" if _VERSION_PRIVATE.startswith("$Format") else _VERSION_PRIVATE.replace("v", "", 1)

# Whether bazel-lib is a pre-release, and therefore has no release artifacts to download.
# NB: When GitHub runs `git archive` to serve a source archive file,
# it honors our .gitattributes and stamps this file, e.g.
# _VERSION_PRIVATE = "v2.0.3-7-g57bfe2c1"
# From https://git-scm.com/docs/git-describe:
# > The "g" prefix stands for "git"
IS_PRERELEASE = VERSION == "0.0.0" or VERSION.find("g") >= 0
