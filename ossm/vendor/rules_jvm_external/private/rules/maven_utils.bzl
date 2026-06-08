load("//private/lib:bzlmod.bzl", "get_module_name_of_owner_of_repo")
load("//private/lib:coordinates.bzl", _unpack_coordinates = "unpack_coordinates")

def unpack_coordinates(coords):
    print("Please load `unpack_coordinates` from `@rules_jvm_external//private/lib:coordinates.bzl`.")
    return _unpack_coordinates(coords)

def parse_exclusions_from_json(json_exclusions):
    """Convert a JSON-encoded exclusion list to group:artifact strings."""
    exclusion_list = json.decode(json_exclusions)
    return ["%s:%s" % (e["group"], e["artifact"]) for e in exclusion_list]

def collect_exclusions_from_maven_infos(maven_infos, ctx):
    """Collect exclusions from MavenInfo objects into a coordinates-keyed dict.

    Args:
        maven_infos: Iterable of MavenInfo objects (typically from all_infos.to_list())
        ctx: Rule context for make variable expansion on coordinates

    Returns:
        Dict mapping maven coordinates to lists of exclusions (group:artifact strings)
    """
    exclusions = {}
    for maven_info in maven_infos:
        if maven_info.coordinates and maven_info.exclusions:
            coords = ctx.expand_make_variables("exclusions", maven_info.coordinates, ctx.var)
            if coords not in exclusions:
                exclusions[coords] = []
            for exclusion in maven_info.exclusions:
                if exclusion not in exclusions[coords]:
                    exclusions[coords].append(exclusion)
    return exclusions

def expand_dict_keys(ctx, d):
    """Expand make variables in dict keys."""
    return {ctx.expand_make_variables("key", k, ctx.var): v for k, v in d.items()}

def merge_and_sort_exclusions(*exclusion_dicts):
    """Merge multiple exclusion dicts, deduplicate entries, and sort."""
    merged = {}
    for d in exclusion_dicts:
        for coords, exclusion_list in d.items():
            if coords not in merged:
                merged[coords] = []
            for e in exclusion_list:
                if e not in merged[coords]:
                    merged[coords].append(e)
    return {k: sorted(v) for k, v in merged.items()}

def process_label_keyed_exclusions(ctx, label_keyed_exclusions, maven_info_provider, label_to_javainfo = None):
    """Process a label-keyed exclusion dict into a coordinates-keyed dict.

    Args:
        ctx: Rule context for make variable expansion
        label_keyed_exclusions: Dict mapping labels to JSON-encoded exclusion lists
        maven_info_provider: The MavenInfo provider to check for coordinates
        label_to_javainfo: Optional dict to validate labels exist in dependencies

    Returns:
        Dict mapping maven coordinates to exclusion lists (group:artifact strings)
    """
    exclusions = {}
    for target, target_exclusions in label_keyed_exclusions.items():
        if label_to_javainfo and not label_to_javainfo.get(target.label):
            print("Warning: exclusions key %s not found in dependencies" % target)
            continue
        if maven_info_provider in target and target[maven_info_provider].coordinates:
            coords = ctx.expand_make_variables("exclusions", target[maven_info_provider].coordinates, ctx.var)
            exclusions[coords] = parse_exclusions_from_json(target_exclusions)
        else:
            print("Warning: exclusions key %s has no maven coordinates" % target.label)
    return exclusions

def _whitespace(indent):
    whitespace = ""
    for i in range(indent):
        whitespace = whitespace + " "
    return whitespace

def format_dep(unpacked, scope = None, indent = 8, include_version = True, exclusions = {}):
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

    if exclusions:
        dependency.extend([
            whitespace,
            "    <exclusions>\n",
        ])
        for exclusion in exclusions:
            group, artifact = exclusion.split(":")
            dependency.extend([
                whitespace,
                "        <exclusion>\n",
                whitespace,
                "            <groupId>%s</groupId>\n" % group,
                whitespace,
                "            <artifactId>%s</artifactId>\n" % artifact,
                whitespace,
                "        </exclusion>\n",
            ])
        dependency.extend([
            whitespace,
            "    </exclusions>\n",
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
        is_bom = False,
        versioned_dep_coordinates = [],
        unversioned_dep_coordinates = [],
        versioned_export_dep_coordinates = [],
        indent = 8,
        exclusions = {}):
    versioned_export_dep_coordinates_set = {
        k: None
        for k in versioned_export_dep_coordinates
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

        # This seems counter-intuitive, but "scope" in Maven and Bazel are different.
        # Bazel -
        #   `deps` are available at compile time and runtime, and are not leaked to dependents
        #   `runtime_deps` are available at runtime, but not compile time, and are not leaked to dependents
        #   `exports` leaked to dependents as compile and runtime dependencies
        # Maven -
        #   `compile` means the dependency is available at compile time and runtime, and is leaked to dependents
        #   `runtime` means the dependency is available at runtime, but not compile time. To promote good build hygiene,
        #             this should be the default scope for the generated pom.xml because it means that we don't
        #             leak our dependencies to dependents.
        # So putting all this together, this should is the mapping:
        # Bazel `deps` -> Maven `runtime`
        # Bazel `runtime_deps` -> Maven `runtime`
        # Bazel `exports` -> Maven `compile`
        # For boms, it seems the best practice is to use the default `compile` scope, unless the dependency is a BOM itself.
        new_scope = "compile" if dep in versioned_export_dep_coordinates_set or is_bom else "runtime"
        if unpacked.packaging == "pom" and is_bom:
            new_scope = "import"

        deps.append(format_dep(unpacked, scope = new_scope, indent = indent, exclusions = exclusions.get(dep, {}), include_version = include_version))

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
