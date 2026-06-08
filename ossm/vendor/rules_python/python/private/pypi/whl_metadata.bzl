"""A simple function to find the METADATA file and parse it"""

_NAME = "Name: "
_PROVIDES_EXTRA = "Provides-Extra: "
_REQUIRES_DIST = "Requires-Dist: "
_VERSION = "Version: "
_CONSOLE_SCRIPTS = "[console_scripts]"

def whl_metadata(*, install_dir, read_fn, logger):
    """Find and parse the METADATA file in the extracted whl contents dir.

    Args:
        install_dir: {type}`path` location where the wheel has been extracted.
        read_fn: the function used to read files.
        logger: the function used to log failures.

    Returns:
        A struct with parsed values:
        * `name`: {type}`str` the name of the wheel.
        * `version`: {type}`str` the version of the wheel.
        * `requires_dist`: {type}`list[str]` the list of requirements.
        * `provides_extra`: {type}`list[str]` the list of extras that this package
          provides.
    """
    metadata_file = find_whl_metadata(install_dir = install_dir, logger = logger)
    contents = read_fn(metadata_file)
    entry_points_file = metadata_file.dirname.get_child("entry_points.txt")
    if entry_points_file.exists:
        entry_points_contents = read_fn(entry_points_file)
    else:
        entry_points_contents = ""

    result = parse_whl_metadata(contents, entry_points_contents)

    if not (result.name and result.version):
        logger.fail("Failed to parse the wheel METADATA file:\n{}\n{}\n{}".format(
            80 * "=",
            contents.rstrip("\n"),
            80 * "=",
        ))
        return None

    return result

def parse_whl_metadata(contents, entry_points_contents = ""):
    """Parse .whl METADATA file

    Args:
        contents: {type}`str` the contents of the file.
        entry_points_contents: {type}`str` the contents of the `entry_points.txt` file if it exists.

    Returns:
        A struct with parsed values:
        * `name`: {type}`str` the name of the wheel.
        * `version`: {type}`str` the version of the wheel.
        * `requires_dist`: {type}`list[str]` the list of requirements.
        * `provides_extra`: {type}`list[str]` the list of extras that this package
          provides.
        * `entry_points`: {type}`list[struct]` the list of
            entry_point metadata.
    """
    parsed = {
        "name": "",
        "provides_extra": [],
        "requires_dist": [],
        "version": "",
    }
    for line in contents.strip().split("\n"):
        if not line:
            # Stop parsing on first empty line, which marks the end of the
            # headers containing the metadata.
            break

        if line.startswith(_NAME):
            _, _, value = line.partition(_NAME)
            parsed["name"] = value.strip()
        elif line.startswith(_VERSION):
            _, _, value = line.partition(_VERSION)
            parsed["version"] = value.strip()
        elif line.startswith(_REQUIRES_DIST):
            _, _, value = line.partition(_REQUIRES_DIST)
            parsed["requires_dist"].append(value.strip(" "))
        elif line.startswith(_PROVIDES_EXTRA):
            _, _, value = line.partition(_PROVIDES_EXTRA)
            parsed["provides_extra"].append(value.strip(" "))

    return struct(
        name = parsed["name"],
        provides_extra = parsed["provides_extra"],
        requires_dist = parsed["requires_dist"],
        version = parsed["version"],
        entry_points = _parse_entry_points(entry_points_contents),
    )

def find_whl_metadata(*, install_dir, logger):
    """Find the whl METADATA file in the install_dir.

    Args:
        install_dir: {type}`path` location where the wheel has been extracted.
        logger: the function used to log failures.

    Returns:
        {type}`path` The path to the METADATA file.
    """
    dist_info = None
    for maybe_dist_info in install_dir.readdir():
        # first find the ".dist-info" folder
        if not (maybe_dist_info.is_dir and maybe_dist_info.basename.endswith(".dist-info")):
            continue

        dist_info = maybe_dist_info
        metadata_file = dist_info.get_child("METADATA")

        if metadata_file.exists:
            return metadata_file

        break

    if dist_info:
        logger.fail("The METADATA file for the wheel could not be found in '{}/{}'".format(install_dir.basename, dist_info.basename))
    else:
        logger.fail("The '*.dist-info' directory could not be found in '{}'".format(install_dir.basename))
    return None

def _parse_entry_points(contents):
    """parse the entry_points.txt file.

    Args:
        contents: {type}`str` The contents of the file

    Returns:
        A list of console_script entry point metadata.
    """
    start = False
    ret = []
    for line in contents.split("\n"):
        line = line.rstrip()

        if line == _CONSOLE_SCRIPTS:
            start = True
            continue

        if not start:
            continue

        if start and line.startswith("["):
            break

        line, _, _comment = line.partition("#")
        line = line.strip()
        if not line:
            continue

        name, _, tail = line.partition("=")

        # importable.module:object.attr
        py_import, _, extras = tail.strip().partition(" ")
        module, _, attribute = py_import.partition(":")

        ret.append(struct(
            name = name.strip(),
            module = module.strip(),
            attribute = attribute.strip(),
            extras = extras.replace(" ", ""),
        ))

    return ret
