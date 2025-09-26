load("@rules_java//java:defs.bzl", "JavaInfo")

MavenInfo = provider(
    fields = {
        # Fields to do with maven coordinates
        "coordinates": "Maven coordinates for the project, which may be None",
        "maven_deps": "Depset of first-order maven dependencies",
        "maven_export_deps": "Depset of first-order maven dependency exports",
        "as_maven_dep": "Depset of this project if used as a maven dependency",

        # Fields used for generating artifacts
        "artifact_infos": "Depset of JavaInfo instances of targets to include in the maven artifact",
        "dep_infos": "Depset of JavaInfo instances of dependencies that the maven artifact depends on",
        "label_to_javainfo": "Dict mapping a label to the JavaInfo that label produces",
        "transitive_exports": "Depset of Labels of exported targets",
    },
)

MavenHintInfo = provider(
    doc = """Provides hints to the `has_maven_deps` aspect about additional dependencies.
This is particularly useful if outputs are generated from aspects, and so may not be able
to offer `tags` to be used to infer maven information.
""",
    fields = {
        "maven_infos": "Depset of MavenInfo instances to also consider as dependencies",
    },
)

_EMPTY_INFO = MavenInfo(
    coordinates = None,
    maven_deps = depset(),
    as_maven_dep = depset(),
    maven_export_deps = depset(),
    artifact_infos = depset(),
    dep_infos = depset(),
    label_to_javainfo = {},
    transitive_exports = depset(),
)

_STOPPED_INFO = MavenInfo(
    coordinates = "STOPPED",
    maven_deps = depset(),
    as_maven_dep = depset(),
    maven_export_deps = depset(),
    artifact_infos = depset(),
    dep_infos = depset(),
    label_to_javainfo = {},
    transitive_exports = depset(),
)

_MAVEN_PREFIX = "maven_coordinates="

_STOP_TAGS = [
    "maven:compile-only",
    "maven:compile_only",
    "no-maven",
]

def read_coordinates(tags):
    coordinates = []
    for stop_tag in _STOP_TAGS:
        if stop_tag in tags:
            return None

    for tag in tags:
        if tag.startswith(_MAVEN_PREFIX):
            coordinates.append(tag[len(_MAVEN_PREFIX):])

    if len(coordinates) > 1:
        fail("Zero or one set of coordinates should be defined: %s" % coordinates)

    if len(coordinates) == 1:
        return coordinates[0]

    return None

_ASPECT_ATTRS = [
    "deps",
    "exports",
    "runtime_deps",
]

def _set_diff(first, second):
    """Returns all items in `first` that are not in `second`"""

    return [item for item in first if item not in second]

def _flatten(array_of_depsets):
    flattened = {}
    for dep in array_of_depsets:
        for item in dep.to_list():
            flattened.update({item: True})

    return flattened.keys()

def calculate_artifact_jars(maven_info):
    """Calculate the actual jars to include in a maven artifact"""
    all_jars = _flatten([i.transitive_runtime_jars for i in maven_info.artifact_infos.to_list()])
    dep_jars = _flatten([i.transitive_runtime_jars for i in maven_info.dep_infos.to_list()])

    return _set_diff(all_jars, dep_jars)

def calculate_artifact_source_jars(maven_info):
    """Calculate the actual jars to include in a maven artifact"""
    all_jars = _flatten([i.transitive_source_jars for i in maven_info.artifact_infos.to_list()])
    dep_jars = _flatten([i.transitive_source_jars for i in maven_info.dep_infos.to_list()])

    return _set_diff(all_jars, dep_jars)

# Used to gather maven data
_gathered = provider(
    fields = [
        "all_infos",
        "export_deps",
        "label_to_javainfo",
        "artifact_infos",
        "transitive_exports",
        "dep_infos",
    ],
)

def _extract_from(gathered, maven_info, dep, is_export_dep):
    java_info = dep[JavaInfo] if dep and JavaInfo in dep else None

    gathered.all_infos.append(maven_info)
    gathered.label_to_javainfo.update(maven_info.label_to_javainfo)

    if is_export_dep:
        gathered.export_deps.append(maven_info)

    if not java_info:
        return

    if maven_info.coordinates:
        gathered.dep_infos.append(dep[JavaInfo])
    else:
        gathered.artifact_infos.append(dep[JavaInfo])
        if is_export_dep:
            gathered.transitive_exports.append(maven_info.transitive_exports)

def _has_maven_deps_impl(target, ctx):
    if not JavaInfo in target:
        return [_EMPTY_INFO]

    # Check the stop tags first to let us exit quickly.
    # When MavenInfo is set, _extract_from will add the dep to the dep_infos list, propagating
    # the dependency info to the pom.xml and excluding its contents from the project-jar.
    # If _EMPTY_INFO is used, _extract_from will add the dep to the artifact_infos list, which
    # will include the contents in the project-jar.
    # If _STOPPED_INFO is used, _extract_from will add the dep to the dep_infos list. This is useful
    # when we want to stop the propagation of the dependency info to the pom.xml while also excluding
    # the jar from the artifact.
    for tag in ctx.rule.attr.tags:
        if tag in _STOP_TAGS:
            return [_STOPPED_INFO]

    coordinates = read_coordinates(ctx.rule.attr.tags)

    gathered = _gathered(
        all_infos = [],
        export_deps = [],
        artifact_infos = [target[JavaInfo]],
        transitive_exports = [],
        dep_infos = [],
        label_to_javainfo = {target.label: target[JavaInfo]},
    )

    for attr in _ASPECT_ATTRS:
        for dep in getattr(ctx.rule.attr, attr, []):
            if MavenHintInfo in dep:
                for info in dep[MavenHintInfo].maven_infos.to_list():
                    _extract_from(gathered, info, None, attr == "exports")

            if not MavenInfo in dep:
                continue

            info = dep[MavenInfo]
            _extract_from(gathered, info, dep, attr == "exports")

    all_infos = gathered.all_infos
    artifact_infos = gathered.artifact_infos
    transitive_exports_from_deps = gathered.transitive_exports
    dep_infos = gathered.dep_infos
    label_to_javainfo = gathered.label_to_javainfo
    maven_deps = depset(transitive = [i.as_maven_dep for i in all_infos])
    maven_export_deps = depset(transitive = [i.as_maven_dep for i in gathered.export_deps])

    transitive_exports_from_exports = depset()
    if hasattr(ctx.rule.attr, "exports"):
        transitive_exports_from_exports = depset(
            [e.label for e in ctx.rule.attr.exports],
            transitive =
                [e[MavenInfo].transitive_exports for e in ctx.rule.attr.exports],
        )

    info = MavenInfo(
        coordinates = coordinates,
        maven_deps = maven_deps,
        maven_export_deps = maven_export_deps,
        as_maven_dep = depset([coordinates]) if coordinates else maven_deps,
        artifact_infos = depset(direct = artifact_infos),
        dep_infos = depset(direct = dep_infos, transitive = [i.dep_infos for i in all_infos]),
        label_to_javainfo = label_to_javainfo,
        transitive_exports = depset(transitive = [transitive_exports_from_exports] + transitive_exports_from_deps),
    )

    return [
        info,
    ]

has_maven_deps = aspect(
    _has_maven_deps_impl,
    attr_aspects = _ASPECT_ATTRS,
)
