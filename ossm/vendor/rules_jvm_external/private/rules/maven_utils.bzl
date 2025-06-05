load("//private/lib:bzlmod.bzl", "get_module_name_of_owner_of_repo")
load("//private/lib:coordinates.bzl", _unpack_coordinates = "unpack_coordinates")

def unpack_coordinates(coords):
    print("Please load `unpack_coordinates` from `@rules_jvm_external//private/lib:coordinates.bzl`.")
    return _unpack_coordinates(coords)

def _whitespace(indent):
    whitespace = ""
    for i in range(indent):
        whitespace = whitespace + " "
    return whitespace

def format_dep(unpacked, scope = None, indent = 8, include_version = True):
    whitespace = _whitespace(indent)

    dependency = [
        whitespace,
        "<dependency>\n",
        whitespace,
        "    <groupId>%s</groupId>\n" % unpacked.group,
        whitespace,
        "    <artifactId>%s</artifactId>\n" % unpacked.artifact,
    ]

    if include_version:
        dependency.extend([
            whitespace,
            "    <version>%s</version>\n" % unpacked.version,
        ])

    if unpacked.classifier and unpacked.classifier != "jar":
        dependency.extend([
            whitespace,
            "    <classifier>%s</classifier>\n" % unpacked.classifier,
        ])

    if unpacked.packaging and unpacked.packaging != "jar":
        dependency.extend([
            whitespace,
            "    <type>%s</type>\n" % unpacked.packaging,
        ])

    if scope and scope != "compile":
        dependency.extend([
            whitespace,
            "    <scope>%s</scope>\n" % scope,
        ])

    dependency.extend([
        whitespace,
        "</dependency>",
    ])

    return "".join(dependency)

def generate_pom(
        ctx,
        coordinates,
        pom_template,
        out_name,
        parent = None,
        versioned_dep_coordinates = [],
        unversioned_dep_coordinates = [],
        versioned_compile_dep_coordinates = [],
        indent = 8):
    versioned_compile_dep_coordinates_set = {
        k: None
        for k in versioned_compile_dep_coordinates
    }
    unpacked_coordinates = _unpack_coordinates(coordinates)
    substitutions = {
        "{groupId}": unpacked_coordinates.group,
        "{artifactId}": unpacked_coordinates.artifact,
        "{version}": unpacked_coordinates.version,
        "{type}": unpacked_coordinates.packaging or "jar",
        "{classifier}": unpacked_coordinates.classifier or "jar",
    }

    if parent:
        # We only want the groupId, artifactID, and version
        unpacked_parent = _unpack_coordinates(parent)

        whitespace = _whitespace(indent - 4)
        parts = [
            whitespace,
            "    <groupId>%s</groupId>\n" % unpacked_parent.group,
            whitespace,
            "    <artifactId>%s</artifactId>\n" % unpacked_parent.artifact,
            whitespace,
            "    <version>%s</version>" % unpacked_parent.version,
        ]
        substitutions.update({"{parent}": "".join(parts)})

    deps = []
    for dep in sorted(versioned_dep_coordinates) + sorted(unversioned_dep_coordinates):
        include_version = dep in versioned_dep_coordinates
        unpacked = _unpack_coordinates(dep)

        new_scope = "compile" if dep in versioned_compile_dep_coordinates_set else "runtime"
        if unpacked.packaging == "pom":
            new_scope = "import"

        deps.append(format_dep(unpacked, scope = new_scope, indent = indent, include_version = include_version))

    substitutions.update({"{dependencies}": "\n".join(deps)})

    out = ctx.actions.declare_file("%s" % out_name)
    ctx.actions.expand_template(
        template = pom_template,
        output = out,
        substitutions = substitutions,
    )

    return out

def determine_additional_dependencies(jar_files, additional_dependencies):
    """Takes a dict of {`Label`: workspace_name} and returns the `Label`s where any `jar_files match a `workspace_name."""
    to_return = []

    for jar in jar_files:
        owner = jar.owner

        # If we can't tell who the owner is, let's assume things are fine
        if not owner:
            continue

        # Users don't know how `bzlmod` mangles workspace names, but we do
        workspace_name = get_module_name_of_owner_of_repo(owner.workspace_name)

        for (dep, name) in additional_dependencies.items():
            if (name == workspace_name) and dep:
                if not dep in to_return:
                    to_return.append(dep)

    return to_return
