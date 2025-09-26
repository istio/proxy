"""Internal functions to parse versions."""

def _safe_int(s, v):
    if not s.isdigit():
        fail("invalid Bazel version '{}': non-numeric segment '{}'".format(v, s))
    return int(s)

def _partition(s):
    for i in range(len(s)):
        c = s[i]
        if c == "-":
            return s[:i], s[i + 1:]
        if not c.isdigit() and c != ".":
            return s[:i], s[i:]
    return s, ""

def parse_version(v):
    """Parses the given Bazel version string into a comparable value.

    Args:
      v (str): version string

    Returns:
      a triple ([major, minor, patch], is_released, prerelease)
    """
    if not v:
        # An empty string is treated as a "dev version", which is greater than anything.
        v = "999999.999999.999999"
    release, prerelease = _partition(v.partition(" ")[0])
    segments = release.split(".")
    if len(segments) != 3:
        fail("invalid Bazel version '{}': got {} dot-separated segments, want 3".format(v, len(segments)))
    if prerelease.startswith("rc") and len(prerelease) > 2 and prerelease[2:].isdigit():
        # Release candidates are considered to be the same as the release version for the purpose of
        # feature detection. This allows for realistic testing of candidates and avoids the need to
        # specify "rc1" on every version.
        prerelease = ""
    return [_safe_int(s, v) for s in segments], not prerelease, prerelease
