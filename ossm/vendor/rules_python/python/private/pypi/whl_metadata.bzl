"""A simple function to find the METADATA file and parse it"""

_NAME = "Name: "
_PROVIDES_EXTRA = "Provides-Extra: "
_REQUIRES_DIST = "Requires-Dist: "
_VERSION = "Version: "

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
    result = parse_whl_metadata(contents)

    if not (result.name and result.version):
        logger.fail("Failed to parsed the wheel METADATA file:\n{}".format(contents))
        return None

    return result

def parse_whl_metadata(contents):
    """Parse .whl METADATA file

    Args:
        contents: {type}`str` the contents of the file.

    Returns:
        A struct with parsed values:
        * `name`: {type}`str` the name of the wheel.
        * `version`: {type}`str` the version of the wheel.
        * `requires_dist`: {type}`list[str]` the list of requirements.
        * `provides_extra`: {type}`list[str]` the list of extras that this package
          provides.
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
