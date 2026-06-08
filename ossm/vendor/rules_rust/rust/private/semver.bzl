"""Semver"""

def semver(version):
    """Constructs a struct containing separated sections of a semantic version value.

    Args:
        version (str): The semver value.

    Returns:
        struct:
            - major (int): The semver's major component. E.g. `1` from `1.2.3`
            - minor (int): The semver's minor component. E.g. `2` from `1.2.3`
            - patch (int): The semver's patch component. E.g. `3` from `1.2.3`
            - pre (optional str): The semver's pre component. E.g. `rc4` from `1.2.3-rc4` or None if absent.
            - str (str): The full string value of the semver.
    """
    parts = version.split(".", 2)
    if len(parts) < 3:
        fail("Unexpected number of parts for semver value: {}".format(version))

    major = parts[0]
    minor = parts[1]
    patch, split, pre = parts[2].partition("-")
    if not split:
        pre = None

    return struct(
        major = int(major),
        minor = int(minor),
        patch = int(patch),
        pre = pre,
        str = version,
    )
