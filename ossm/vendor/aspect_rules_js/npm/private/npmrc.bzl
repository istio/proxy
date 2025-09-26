"""npmrc utils"""

def parse_npmrc(npmrc_content):
    """Parse an `.npmrc` file in into key/value map.

    `.npmrc` files are in [INI](https://en.wikipedia.org/wiki/INI_file#Format) format but we don't
    treat keys as [case insensitive](https://en.wikipedia.org/wiki/INI_file#Case_sensitivity) due
    to https://github.com/aspect-build/rules_js/issues/622.

    Duplicate case-sensitive keys override previous values.

    Supports:
    * basic key/value
    * # or ; comments

    Does NOT support or ignores:
    * sections
    * escape characters
    * number or boolean types (all values are strings)
    * comment characters (#, ;) within a value

    Args:
        npmrc_content: the `.npmrc` content string

    Returns:
        A dict() of key/value pairs of the `.npmrc` properties
    """

    props = []

    for line in npmrc_content.splitlines():
        line = line.strip()

        # Ignore sections
        if line.startswith("["):
            continue

        # Strip comments
        line = line.split(";", 2)[0]
        line = line.split("#", 2)[0]

        # Empty or was all comments
        if len(line) == 0:
            continue

        p = line.partition("=")
        name = p[0].strip()
        value = p[2].strip()

        if len(value) > 1 and (value[0] == "\"" or value[0] == "'") and value[0] == value[-1]:
            value = value[1:-1]

        props.append([name, value])

    return dict(props)
