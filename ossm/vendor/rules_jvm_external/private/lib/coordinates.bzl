SUPPORTED_PACKAGING_TYPES = [
    "aar",
    "bundle",
    "dll",
    "dylib",
    "eclipse-plugin",
    "exe",
    "hk2-jar",
    "jar",
    "json",
    "maven-plugin",
    "orbit",
    "pom",
    "scala-jar",
    "so",
    "test-jar",
]

def unpack_coordinates(coords):
    """Takes a maven coordinate and unpacks it into a struct with fields
    `group`, `artifact`, `version`, `packaging`, `classifier`
    where `version,` `packaging` and `classifier` may be `None`

    Assumes `coords` is in one of the following syntaxes:
     * group:artifact[:packaging[:classifier]]:version
     * group:artifact[:version][:classifier][@packaging]
    """
    if not coords:
        return None

    if type(coords) == "dict":
        return struct(
            group = coords["group"],
            artifact = coords["artifact"],
            version = coords.get("version", ""),
            packaging = coords.get("packaging", None),
            classifier = coords.get("classifier", None),
        )

    pieces = coords.split(":")
    if len(pieces) < 2:
        fail("Could not parse maven coordinate: %s" % coords)
    group = pieces[0]
    artifact = pieces[1]

    if len(pieces) == 2:
        return struct(group = group, artifact = artifact, version = "", packaging = None, classifier = None)

    # Strictly speaking this could be one of `g:a:p` or `g:a:v` but realistically, it's going to be
    # a regular `g:a:v` triplet. If that's not what someone wants, they'll need to qualify the
    # type in some other way
    if len(pieces) == 3:
        version = pieces[2]
        packaging = None
        if "@" in pieces[2]:
            (version, packaging) = pieces[2].split("@", 2)

        return struct(group = group, artifact = artifact, version = version, packaging = packaging, classifier = None)

    # Unambiguously the original format
    if len(pieces) == 5:
        packaging = pieces[2]
        classifier = pieces[3]
        version = pieces[4]
        return struct(group = group, artifact = artifact, packaging = packaging, classifier = classifier, version = version)

    if len(pieces) == 4:
        # Either g:a:p:v or g:a:v:c or g:a:v:c@p.

        # Handle the easy case first
        if "@" in pieces[3]:
            (classifier, packaging) = pieces[3].split("@", 2)
            version = pieces[2]
            return struct(group = group, artifact = artifact, packaging = packaging, classifier = classifier, version = version)

        # Experience has show us that versions can be almost anything, but there's a relatively small
        # pool of packaging types that people use. This isn't a perfect heuristic, but it's Good
        # Enough for us right now. Previously, we attempted to figure out if the `piece[2]` was a
        # version by checking to see whether it's a number. Foolish us.

        if pieces[2] in SUPPORTED_PACKAGING_TYPES:
            packaging = pieces[2]
            version = pieces[3]
            rewritten = "%s:%s:%s@%s" % (group, artifact, version, packaging)
            print("Assuming %s should be interpreted as %s" % (coords, rewritten))
            return struct(group = group, artifact = artifact, packaging = packaging, version = version, classifier = None)

        # We could still be in one of `g:a:p:v` or `g:a:v:c`, but it's likely the latter. I do not
        # think there are any packaging formats commonly in use in the maven ecosystem that contain
        # numbers, so we'll check to see if `pieces[2]` contains one or more numbers and use that to
        # decide. This allows us to cope with packaging formats we've not seen before, such as the
        # `packaging` we use in our own tests.
        digit_count = len([c for c in pieces[2].elems() if c.isdigit()])

        if digit_count:
            version = pieces[2]
            classifier = pieces[3]
            return struct(group = group, artifact = artifact, classifier = classifier, version = version, packaging = None)
        else:
            return struct(group = group, artifact = artifact, classifier = None, version = pieces[3], packaging = pieces[2])

    fail("Could not parse maven coordinate: %s" % coords)

def to_external_form(coords):
    """Formats `coords` as a string suitable for use by tools such as Gradle.

    The returned format matches Gradle's "external dependency" short-form
    syntax: `group:name:version:classifier@packaging`
    """

    if type(coords) == "string":
        unpacked = unpack_coordinates(coords)
    elif type(coords) == "dict":
        # Ensures that we have all the fields we expect to be present
        fully_populated = {"version": None, "packaging": None, "classifier": None} | coords
        unpacked = struct(**fully_populated)
    else:
        unpacked = coords

    to_return = "%s:%s:%s" % (unpacked.group, unpacked.artifact, unpacked.version)

    if hasattr(unpacked, "classifier"):
        if unpacked.classifier and unpacked.classifier != "jar":
            to_return += ":" + unpacked.classifier

    if hasattr(unpacked, "packaging"):
        if unpacked.packaging and unpacked.packaging != "jar":
            to_return += "@" + unpacked.packaging

    return to_return

_DEFAULT_PURL_REPOS = [
    "https://repo.maven.apache.org/maven2",
    "https://repo.maven.apache.org/maven2/",
    "https://repo1.maven.org",
    "https://repo1.maven.org/",
]

def to_purl(coords, repository):
    to_return = "pkg:maven/"

    unpacked = unpack_coordinates(coords)
    to_return += "{group}:{artifact}@{version}".format(
        artifact = unpacked.artifact,
        group = unpacked.group,
        version = unpacked.version,
    )

    suffix = []
    if unpacked.classifier:
        suffix.append("classifier=" + unpacked.classifier)
    if unpacked.packaging and "jar" != unpacked.packaging:
        suffix.append("type=" + unpacked.packaging)
    if repository and repository not in _DEFAULT_PURL_REPOS:
        # Default repository name is pulled from https://github.com/package-url/purl-spec/blob/master/PURL-TYPES.rst
        suffix.append("repository=" + repository)

    if len(suffix):
        to_return += "?" + "&".join(suffix)

    return to_return
