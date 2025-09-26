"Select a single wheel that fits the parameters of a target platform."

load("//python/private:version.bzl", "version")
load(":parse_whl_name.bzl", "parse_whl_name")
load(":python_tag.bzl", "PY_TAG_GENERIC", "python_tag")

_ANDROID = "android"
_IOS = "ios"
_MANYLINUX = "manylinux"
_MACOSX = "macosx"
_MUSLLINUX = "musllinux"

# Taken from https://peps.python.org/pep-0600/
_LEGACY_ALIASES = {
    "manylinux1_i686": "manylinux_2_5_i686",
    "manylinux1_x86_64": "manylinux_2_5_x86_64",
    "manylinux2010_i686": "manylinux_2_12_i686",
    "manylinux2010_x86_64": "manylinux_2_12_x86_64",
    "manylinux2014_aarch64": "manylinux_2_17_aarch64",
    "manylinux2014_armv7l": "manylinux_2_17_armv7l",
    "manylinux2014_i686": "manylinux_2_17_i686",
    "manylinux2014_ppc64": "manylinux_2_17_ppc64",
    "manylinux2014_ppc64le": "manylinux_2_17_ppc64le",
    "manylinux2014_s390x": "manylinux_2_17_s390x",
    "manylinux2014_x86_64": "manylinux_2_17_x86_64",
}

def _value_priority(*, tag, values):
    keys = []
    for priority, wp in enumerate(values):
        if tag == wp:
            keys.append(priority)

    return max(keys) if keys else None

def _is_platform_tag_versioned(tag):
    return (
        tag.startswith(_ANDROID) or
        tag.startswith(_IOS) or
        tag.startswith(_MACOSX) or
        tag.startswith(_MANYLINUX) or
        tag.startswith(_MUSLLINUX)
    )

def _parse_platform_tags(tags):
    """A helper function that parses all of the platform tags.

    The main idea is to make this more robust and have better debug messages about which will
    is compatible and which is not with the target platform.
    """
    ret = []
    replacements = {}
    for tag in tags:
        tag = _LEGACY_ALIASES.get(tag, tag)

        if not _is_platform_tag_versioned(tag):
            ret.append(tag)
            continue

        want_os, sep, tail = tag.partition("_")
        if not sep:
            fail("could not parse the tag: {}".format(tag))

        want_major, _, tail = tail.partition("_")
        if want_major == "*":
            # the expected match is any version
            want_arch = tail
        elif want_os.startswith(_ANDROID):
            want_arch = tail
        else:
            # drop the minor version segment
            _, _, want_arch = tail.partition("_")

        placeholder = "{}_*_{}".format(want_os, want_arch)
        replacements[placeholder] = tag
        if placeholder in ret:
            ret.remove(placeholder)

        ret.append(placeholder)

    return [
        replacements.get(p, p)
        for p in ret
    ]

def _platform_tag_priority(*, tag, values):
    # Implements matching platform tag
    # https://packaging.python.org/en/latest/specifications/platform-compatibility-tags/

    if not _is_platform_tag_versioned(tag):
        res = _value_priority(tag = tag, values = values)
        if res == None:
            return res

        return (res, (0, 0))

    # Only android, ios, macosx, manylinux or musllinux platforms should be considered

    os, _, tail = tag.partition("_")
    major, _, tail = tail.partition("_")
    if not tag.startswith(_ANDROID):
        minor, _, arch = tail.partition("_")
    else:
        minor = "0"
        arch = tail
    version = (int(major), int(minor))

    keys = []
    for priority, wp in enumerate(values):
        want_os, sep, tail = wp.partition("_")
        if not sep:
            # if there is no `_` separator, then it means that we have something like `win32` or
            # similar wheels that we are considering, this means that it should be discarded because
            # we are dealing only with platforms that have `_`.
            continue

        if want_os != os:
            # os should always match exactly for us to match and assign a priority
            continue

        want_major, _, tail = tail.partition("_")
        if want_major == "*":
            # the expected match is any version
            want_major = ""
            want_minor = ""
            want_arch = tail
        elif tag.startswith(_ANDROID):
            # we set it to `0` above, so setting the `want_minor` her to `0` will make things
            # consistent.
            want_minor = "0"
            want_arch = tail
        else:
            # here we parse the values from the given platform
            want_minor, _, want_arch = tail.partition("_")

        if want_arch != arch:
            # the arch should match exactly
            continue

        # if want_major is defined, then we know that we don't have a `*` in the matcher.
        want_version = (int(want_major), int(want_minor)) if want_major else None
        if not want_version or version <= want_version:
            keys.append((priority, (-version[0], -version[1])))

    return max(keys) if keys else None

def _python_tag_priority(*, tag, implementation, py_version):
    if tag.startswith(PY_TAG_GENERIC):
        ver_str = tag[len(PY_TAG_GENERIC):]
    elif tag.startswith(implementation):
        ver_str = tag[len(implementation):]
    else:
        return None

    # Add a 0 at the end in case it is a single digit
    ver_str = "{}.{}".format(ver_str[0], ver_str[1:] or "0")

    ver = version.parse(ver_str)
    if not version.is_compatible(py_version, ver):
        return None

    return (
        tag.startswith(implementation),
        version.key(ver),
    )

def _candidates_by_priority(
        *,
        whls,
        implementation_name,
        python_version,
        whl_abi_tags,
        whl_platform_tags,
        logger):
    """Calculate the priority of each wheel

    Returns:
        A dictionary where keys are priority tuples which allows us to sort and pick the
        last item.
    """
    py_version = version.parse(python_version, strict = True)
    implementation = python_tag(implementation_name)

    ret = {}
    for whl in whls:
        parsed = parse_whl_name(whl.filename)
        priority = None

        # See https://packaging.python.org/en/latest/specifications/platform-compatibility-tags/#compressed-tag-sets
        for platform in parsed.platform_tag.split("."):
            platform = _platform_tag_priority(tag = platform, values = whl_platform_tags)
            if platform == None:
                logger.debug(lambda: "The platform_tag in '{}' does not match given list: {}".format(
                    whl.filename,
                    whl_platform_tags,
                ))
                continue

            for py in parsed.python_tag.split("."):
                py = _python_tag_priority(
                    tag = py,
                    implementation = implementation,
                    py_version = py_version,
                )
                if py == None:
                    logger.debug(lambda: "The python_tag in '{}' does not match implementation or version: {} {}".format(
                        whl.filename,
                        implementation,
                        py_version.string,
                    ))
                    continue

                for abi in parsed.abi_tag.split("."):
                    abi = _value_priority(
                        tag = abi,
                        values = whl_abi_tags,
                    )
                    if abi == None:
                        logger.debug(lambda: "The abi_tag in '{}' does not match given list: {}".format(
                            whl.filename,
                            whl_abi_tags,
                        ))
                        continue

                    # 1. Prefer platform wheels
                    # 2. Then prefer implementation/python version
                    # 3. Then prefer more specific ABI wheels
                    candidate = (platform, py, abi)
                    priority = priority or candidate
                    if candidate > priority:
                        priority = candidate

        if priority == None:
            logger.debug(lambda: "The whl '{}' is incompatible".format(
                whl.filename,
            ))
            continue

        ret[priority] = whl

    return ret

def select_whl(
        *,
        whls,
        python_version,
        whl_platform_tags,
        whl_abi_tags,
        implementation_name = "cpython",
        limit = 1,
        logger):
    """Select a whl that is the most suitable for the given platform.

    Args:
        whls: {type}`list[struct]` a list of candidates which have a `filename`
            attribute containing the `whl` filename.
        python_version: {type}`str` the target python version.
        implementation_name: {type}`str` the `implementation_name` from the target_platform env.
        whl_abi_tags: {type}`list[str]` The whl abi tags to select from. The preference is
            for wheels that have ABI values appearing later in the `whl_abi_tags` list.
        whl_platform_tags: {type}`list[str]` The whl platform tags to select from.
            The platform tag may contain `*` and this means that if the platform tag is
            versioned (e.g. `manylinux`), then we will select the highest available
            platform version, e.g. if `manylinux_2_17` and `manylinux_2_5` wheels are both
            compatible, we will select `manylinux_2_17`. Otherwise for versioned platform
            tags we select the highest *compatible* version, e.g. if `manylinux_2_6`
            support is requested, then we would select `manylinux_2_5` in the previous
            example. This allows us to pass the same filtering parameters when selecting
            all of the whl dependencies irrespective of what actual platform tags they
            contain.
        limit: {type}`int` number of wheels to return. Defaults to 1.
        logger: {type}`struct` the logger instance.

    Returns:
        {type}`list[struct] | struct | None`, a single struct from the `whls` input
            argument or `None` if a match is not found. If the `limit` is greater than
            one, then we will return a list.
    """
    candidates = _candidates_by_priority(
        whls = whls,
        implementation_name = implementation_name,
        python_version = python_version,
        whl_abi_tags = whl_abi_tags,
        whl_platform_tags = _parse_platform_tags(whl_platform_tags),
        logger = logger,
    )

    if not candidates:
        return None

    res = [i[1] for i in sorted(candidates.items())]
    logger.debug(lambda: "Sorted candidates:\n{}".format(
        "\n".join([c.filename for c in res]),
    ))

    return res[-1] if limit == 1 else res[-limit:]
