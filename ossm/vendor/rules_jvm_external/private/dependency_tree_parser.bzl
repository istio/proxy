# Copyright 2019 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and

"""A parser of the dependency tree from Coursier or maven_install.json.

This file contains parsing functions to turn a JSON-like dependency tree
into target declarations (jvm_import) for the final @maven//:BUILD file.
"""

load(
    "//private:coursier_utilities.bzl",
    "PLATFORM_CLASSIFIER",
    "escape",
    "get_classifier",
    "get_packaging",
    "is_maven_local_path",
    "match_group_and_artifact",
    "strip_packaging_and_classifier",
    "strip_packaging_and_classifier_and_version",
    "to_repository_name",
)
load("//private/lib:coordinates.bzl", "unpack_coordinates")

def _genrule_copy_artifact_from_http_file(artifact, visibilities):
    http_file_repository = to_repository_name(artifact["coordinates"])

    file = artifact.get("out", artifact["file"])

    genrule = [
        "copy_file(",
        "     name = \"%s_extension\"," % http_file_repository,
        "     src = \"@%s//file\"," % http_file_repository,
        "     out = \"%s\"," % file,
        # Windows doesn't care about the executable bit for executables, but copy_file
        # will fail in some cases (disable this line and run ` bazel test //tests/unit/build_tests:all_artifacts`
        # to see this failing
        "     allow_symlink = %s," % ("False" if file.endswith(".exe") else "True"),
    ]
    if get_packaging(artifact["coordinates"]) == "exe":
        genrule.append("     is_executable = True,")
    genrule.extend([
        "     visibility = [%s]" % (",".join(["\"%s\"" % v for v in visibilities])),
        ")",
    ])
    return "\n".join(genrule)

def _deduplicate_list(items):
    seen_items = {}
    unique_items = []
    for item in items:
        if item not in seen_items:
            seen_items[item] = True
            unique_items.append(item)
    return unique_items

def _find_repository_url(artifact_url, repositories):
    longest_match = None
    for repository in repositories:
        if artifact_url.startswith(repository):
            if len(repository) > len(longest_match or ""):
                longest_match = repository
    return longest_match

def _get_maven_url(artifact_urls):
    if len(artifact_urls) == 0:
        return None

    # We want to use the Maven Central repo if it's there
    # since so much of the world expects that.
    for url in artifact_urls:
        if url.startswith("https://repo1.maven.org/maven2/"):
            return url

    # Return anything
    return artifact_urls[0]

def _generate_target(
        repository_ctx,
        jar_versionless_target_labels,
        explicit_artifacts,
        srcjar_paths,
        labels_to_override,
        repository_urls,
        neverlink_artifacts,
        testonly_artifacts,
        default_visibilities,
        artifact):
    to_return = []
    simple_coord = strip_packaging_and_classifier_and_version(artifact["coordinates"])
    target_label = escape(simple_coord)
    artifact_path = artifact["file"]

    # 1. Generate the rule class.
    #
    # (jvm|aar)_import(
    #
    packaging = artifact_path.split(".").pop()
    if packaging == "jar":
        # Regular `java_import` invokes ijar on all JARs, causing some Scala and
        # Kotlin compile interface JARs to be incorrect. We replace java_import
        # with a simple jvm_import Starlark rule that skips ijar.
        import_rule = "jvm_import"
        jar_versionless_target_labels.append(target_label)
    elif packaging == "aar":
        import_rule = "aar_import"
        jar_versionless_target_labels.append(target_label)
    elif packaging in ["dylib", "so", "dll"]:
        import_rule = "java_library"
    else:
        fail("Unsupported packaging type: " + packaging)

    target_import_string = [import_rule + "("]

    # 2. Generate the target label.
    #
    # java_import(
    # 	name = "org_hamcrest_hamcrest_library",
    #
    target_import_string.append("\tname = \"%s\"," % target_label)

    # 3. Generate the jars/aar attribute to the relative path of the artifact.
    #    Optionally generate srcjar attr too.
    #
    #
    # java_import(
    # 	name = "org_hamcrest_hamcrest_library",
    # 	jars = ["https/repo1.maven.org/maven2/org/hamcrest/hamcrest-library/1.3/hamcrest-library-1.3.jar"],
    # 	srcjar = "https/repo1.maven.org/maven2/org/hamcrest/hamcrest-library/1.3/hamcrest-library-1.3-sources.jar",
    #
    is_dylib = False
    if packaging == "jar":
        target_import_string.append("\tjar = \"%s\"," % artifact_path)
        if srcjar_paths != None and target_label in srcjar_paths:
            target_import_string.append("\tsrcjar = \"%s\"," % srcjar_paths[target_label])
    elif packaging == "aar":
        target_import_string.append("\taar = \"%s\"," % artifact_path)
        if srcjar_paths != None and target_label in srcjar_paths:
            target_import_string.append("\tsrcjar = \"%s\"," % srcjar_paths[target_label])
    elif packaging in ["so", "dylib", "dll"]:
        is_dylib = True
        jar_versionless_target_labels.append(target_label)
        dylib = simple_coord.split(":")[-1] + "." + packaging
        to_return.append(
            """
copy_file(
    name = "{dylib}_extension",
    src = "@{repository}//file",
    out = "{dylib}",
    allow_symlink = True,
    visibility = ["//visibility:public"],
)""".format(
                dylib = dylib,
                repository = to_repository_name(artifact["coordinates"]),
            ),
        )

    # 4. Generate the deps attribute with references to other target labels.
    #
    # java_import(
    # 	name = "org_hamcrest_hamcrest_library",
    # 	jars = ["https/repo1.maven.org/maven2/org/hamcrest/hamcrest-library/1.3/hamcrest-library-1.3.jar"],
    # 	srcjar = "https/repo1.maven.org/maven2/org/hamcrest/hamcrest-library/1.3/hamcrest-library-1.3-sources.jar",
    # 	deps = [
    # 		":org_hamcrest_hamcrest_core",
    # 	],
    #
    if is_dylib:
        target_import_string.append("\truntime_deps = [")
    else:
        target_import_string.append("\tdeps = [")

    # Dedupe dependencies here. Sometimes coursier will return "x.y:z:aar:version" and "x.y:z:version" in the
    # same list of dependencies.
    target_import_labels = []
    for dep in artifact["deps"]:
        if get_packaging(dep) == "json":
            continue
        stripped_dep = strip_packaging_and_classifier_and_version(dep)
        dep_target_label = escape(stripped_dep)

        # If we have matching artifacts with platform classifiers, skip adding this dependency.
        # See https://github.com/bazelbuild/rules_jvm_external/issues/686
        if match_group_and_artifact(artifact["coordinates"], dep) and \
           get_classifier(artifact["coordinates"]) in PLATFORM_CLASSIFIER and \
           get_classifier(dep) in PLATFORM_CLASSIFIER:
            continue

        # Coursier returns cyclic dependencies sometimes. Handle it here.
        # See https://github.com/bazelbuild/rules_jvm_external/issues/172
        if dep_target_label != target_label:
            if dep_target_label in labels_to_override:
                dep_target_label = labels_to_override.get(dep_target_label)
            else:
                dep_target_label = ":" + dep_target_label
            target_import_labels.append("\t\t\"%s\",\n" % dep_target_label)
    target_import_labels = _deduplicate_list(target_import_labels)

    target_import_string.append("".join(target_import_labels) + "\t],")

    # 5. Add a tag with the original maven coordinates for use generating pom files
    # For use with this rule https://github.com/google/bazel-common/blob/f1115e0f777f08c3cdb115526c4e663005bec69b/tools/maven/pom_file.bzl#L177
    #
    # java_import(
    # 	name = "org_hamcrest_hamcrest_library",
    # 	jars = ["https/repo1.maven.org/maven2/org/hamcrest/hamcrest-library/1.3/hamcrest-library-1.3.jar"],
    # 	srcjar = "https/repo1.maven.org/maven2/org/hamcrest/hamcrest-library/1.3/hamcrest-library-1.3-sources.jar",
    # 	deps = [
    # 		":org_hamcrest_hamcrest_core",
    # 	],
    #   tags = [
    #       "maven_coordinates=org.hamcrest:hamcrest.library:1.3"],
    #       "maven_url=https://repo1.maven.org/maven/org/hamcrest/hamcrest-core/1.3/hamcrest-core-1.3.jar",
    #   ],

    coordinates = artifact.get("maven_coordinates", artifact["coordinates"])
    maven_url = _get_maven_url(artifact["urls"])

    target_import_string.append("\ttags = [")
    target_import_string.append("\t\t\"maven_coordinates=%s\"," % coordinates)
    if len(artifact["urls"]):
        target_import_string.append("\t\t\"maven_url=%s\"," % maven_url)
        repository_url = _find_repository_url(maven_url, repository_urls)
        if repository_url:
            target_import_string.append("\t\t\"maven_repository=%s\"," % repository_url)
    else:
        target_import_string.append("\t\t\"maven_url=None\",")
    if neverlink_artifacts.get(simple_coord):
        target_import_string.append("\t\t\"maven:compile-only\",")
    if artifact.get("sha256"):
        target_import_string.append("\t\t\"maven_sha256=%s\"," % artifact["sha256"])
    target_import_string.append("\t],")

    if packaging == "jar":
        target_import_string.append("\tmaven_coordinates = \"%s\"," % coordinates)
        if len(artifact["urls"]):
            target_import_string.append("\tmaven_url = \"%s\"," % maven_url)
    else:
        unpacked = unpack_coordinates(coordinates)
        url = maven_url if len(artifact["urls"]) else None

        package_info_name = "%s_package_info" % target_label
        target_import_string.append("\tapplicable_licenses = [\":%s\"]," % package_info_name)
        to_return.append("""
package_info(
    name = {name},
    package_name = {coordinates},
    package_url = {url},
    package_version = {version},
)
""".format(
            coordinates = repr(coordinates),
            name = repr(package_info_name),
            url = repr(url),
            version = repr(unpacked.version),
        ))

    # 6. If `neverlink` is True in the artifact spec, add the neverlink attribute to make this artifact
    #    available only as a compile time dependency.
    #
    # java_import(
    # 	name = "org_hamcrest_hamcrest_library",
    # 	jars = ["https/repo1.maven.org/maven2/org/hamcrest/hamcrest-library/1.3/hamcrest-library-1.3.jar"],
    # 	srcjar = "https/repo1.maven.org/maven2/org/hamcrest/hamcrest-library/1.3/hamcrest-library-1.3-sources.jar",
    # 	deps = [
    # 		":org_hamcrest_hamcrest_core",
    # 	],
    #   tags = [
    #       "maven_coordinates=org.hamcrest:hamcrest.library:1.3"],
    #       "maven_url=https://repo1.maven.org/maven/org/hamcrest/hamcrest-core/1.3/hamcrest-core-1.3.jar",
    #       "maven:compile-only",
    #   ],
    #   neverlink = True,
    if neverlink_artifacts.get(simple_coord):
        target_import_string.append("\tneverlink = True,")

    # 7. If `testonly` is True in the artifact spec, add the testonly attribute to make this artifact
    #    available only as a test dependency.
    #
    # java_import(
    #   name = "org_hamcrest_hamcrest_library",
    #   jars = ["https/repo1.maven.org/maven2/org/hamcrest/hamcrest-library/1.3/hamcrest-library-1.3.jar"],
    #   srcjar = "https/repo1.maven.org/maven2/org/hamcrest/hamcrest-library/1.3/hamcrest-library-1.3-sources.jar",
    #   deps = [
    #       ":org_hamcrest_hamcrest_core",
    #   ],
    #   tags = [
    #       "maven_coordinates=org.hamcrest:hamcrest.library:1.3"],
    #       "maven_url=https://repo1.maven.org/maven/org/hamcrest/hamcrest-core/1.3/hamcrest-core-1.3.jar",
    #       "maven:compile-only",
    #   ],
    #   neverlink = True,
    #   testonly = True,
    if testonly_artifacts.get(simple_coord):
        target_import_string.append("\ttestonly = True,")

    # 8. If `strict_visibility` is True in the artifact spec, define public
    #    visibility only for non-transitive dependencies.
    #
    # java_import(
    # 	name = "org_hamcrest_hamcrest_library",
    # 	jars = ["https/repo1.maven.org/maven2/org/hamcrest/hamcrest-library/1.3/hamcrest-library-1.3.jar"],
    # 	srcjar = "https/repo1.maven.org/maven2/org/hamcrest/hamcrest-library/1.3/hamcrest-library-1.3-sources.jar",
    # 	deps = [
    # 		":org_hamcrest_hamcrest_core",
    # 	],
    #   tags = [
    #       "maven_coordinates=org.hamcrest:hamcrest.library:1.3"],
    #       "maven_url=https://repo1.maven.org/maven/org/hamcrest/hamcrest-core/1.3/hamcrest-core-1.3.jar",
    #       "maven:compile-only",
    #   ],
    #   neverlink = True,
    #   testonly = True,
    #   visibility = ["//visibility:public"],
    target_visibilities = []
    if not repository_ctx.attr.strict_visibility or explicit_artifacts.get(simple_coord):
        target_visibilities.append("//visibility:public")
    elif repository_ctx.attr.generate_compat_repositories:
        target_visibilities.append("@%s//:__subpackages__" % target_label)
    else:
        target_visibilities.extend(repository_ctx.attr.strict_visibility_value)

    target_import_string.append("\tvisibility = [%s]," % (",".join(["\"%s\"" % t for t in target_visibilities])))
    alias_visibility = "\tvisibility = [%s],\n" % (",".join(["\"%s\"" % t for t in target_visibilities]))

    # 9. Finish the java_import rule.
    #
    # java_import(
    # 	name = "org_hamcrest_hamcrest_library",
    # 	jars = ["https/repo1.maven.org/maven2/org/hamcrest/hamcrest-library/1.3/hamcrest-library-1.3.jar"],
    # 	srcjar = "https/repo1.maven.org/maven2/org/hamcrest/hamcrest-library/1.3/hamcrest-library-1.3-sources.jar",
    # 	deps = [
    # 		":org_hamcrest_hamcrest_core",
    # 	],
    #   tags = [
    #       "maven_coordinates=org.hamcrest:hamcrest.library:1.3"],
    #       "maven_url=https://repo1.maven.org/maven/org/hamcrest/hamcrest-core/1.3/hamcrest-core-1.3.jar",
    #       "maven:compile-only",
    #   ],
    #   neverlink = True,
    #   testonly = True,
    # )
    target_import_string.append(")")

    to_return.append("\n".join(target_import_string))

    # 10. Create a versionless alias target
    #
    # alias(
    #   name = "org_hamcrest_hamcrest_library_1_3",
    #   actual = "org_hamcrest_hamcrest_library",
    # )
    versioned_target_alias_label = escape(strip_packaging_and_classifier(artifact["coordinates"]))
    to_return.append("alias(\n\tname = \"%s\",\n\tactual = \"%s\",\n%s)" %
                     (versioned_target_alias_label, target_label, alias_visibility))

    for annotation_processor in artifact.get("annotation_processors", []):
        to_return.append(
            """java_plugin(
\tname = "{name}",{testonly}
\tdeps = [":{jar_target}"],
\tgenerates_api = True,
processor_class = "{processor_class}",
{alias_visibility})""".format(
                name = "{}__java_plugin__{}".format(target_label, escape(annotation_processor)),
                testonly = "\n\ttestonly = True," if testonly_artifacts.get(simple_coord) else "",
                jar_target = target_label,
                processor_class = annotation_processor,
                alias_visibility = alias_visibility,
            ),
        )

    # 11. If using maven_install.json, use a genrule to copy the file from the http_file
    # repository into this repository.
    #
    # genrule(
    #     name = "org_hamcrest_hamcrest_library_1_3_extension",
    #     srcs = ["@org_hamcrest_hamcrest_library_1_3//file"],
    #     outs = ["@maven//:v1/https/repo1.maven.org/maven2/org/hamcrest/hamcrest-library/1.3/hamcrest-library-1.3.jar"],
    #     cmd = "cp $< $@",
    # )
    if repository_ctx.attr.maven_install_json:
        to_return.append(_genrule_copy_artifact_from_http_file(artifact, default_visibilities))

    return to_return

# Generate BUILD file with jvm_import and aar_import for each artifact in
# the transitive closure, with their respective deps mapped to the resolved
# tree.
#
# Made function public for testing.
def _generate_imports(repository_ctx, dependencies, explicit_artifacts, neverlink_artifacts, testonly_artifacts, override_targets, skip_maven_local_dependencies):
    repository_urls = [json.decode(repository)["repo_url"] for repository in repository_ctx.attr.repositories]

    # The list of java_import/aar_import declaration strings to be joined at the end
    all_imports = []

    # A dictionary (set) of coordinates. This is to ensure we don't generate
    # duplicate labels
    #
    # seen_imports :: string -> bool
    seen_imports = {}

    # A list of versionless target labels for jar artifacts. This is used for
    # generating a compatibility layer for repositories. For example, if we generate
    # @maven//:junit_junit, we also generate @junit_junit//jar as an alias to it.
    jar_versionless_target_labels = []

    labels_to_override = {}
    for coord in override_targets:
        labels_to_override.update({escape(coord): override_targets.get(coord)})

    default_visibilities = repository_ctx.attr.strict_visibility_value if repository_ctx.attr.strict_visibility else ["//visibility:public"]

    # First collect a map of target_label to their srcjar relative paths, and symlink the srcjars if needed.
    # We will use this map later while generating target declaration strings with the "srcjar" attr.
    srcjar_paths = None
    if repository_ctx.attr.fetch_sources:
        srcjar_paths = {}
        for artifact in dependencies:
            if get_classifier(artifact["coordinates"]) == "sources":
                artifact_path = artifact["file"]

                # Skip the maven local dependencies if requested
                if skip_maven_local_dependencies and is_maven_local_path(artifact_path):
                    continue
                if artifact_path != None and artifact_path not in seen_imports:
                    seen_imports[artifact_path] = True
                    target_label = escape(strip_packaging_and_classifier_and_version(artifact["coordinates"]))
                    srcjar_paths[target_label] = artifact_path
                    if repository_ctx.attr.maven_install_json:
                        all_imports.append(_genrule_copy_artifact_from_http_file(artifact, default_visibilities))

    # Iterate through the list of artifacts, and generate the target declaration strings.
    for artifact in dependencies:
        artifact_path = artifact["file"]

        # Skip the maven local dependencies if requested
        if skip_maven_local_dependencies and is_maven_local_path(artifact_path):
            continue
        simple_coord = strip_packaging_and_classifier_and_version(artifact["coordinates"])
        packaging = get_packaging(artifact["coordinates"])
        target_label = escape(simple_coord)
        alias_visibility = ""

        if target_label in seen_imports:
            # Skip if we've seen this target label before. Every versioned artifact is uniquely mapped to a target label.
            pass
        elif repository_ctx.attr.fetch_sources and get_classifier(artifact["coordinates"]) == "sources":
            # We already processed the sources above, so skip them here.
            pass
        elif repository_ctx.attr.fetch_javadoc and get_classifier(artifact["coordinates"]) == "javadoc":
            seen_imports[target_label] = True
            all_imports.append(
                "filegroup(\n\tname = \"%s\",\n\tsrcs = [\"%s\"],\n\ttags = [\"javadoc\"],\n\tvisibility = [\"//visibility:public\"],\n)" % (target_label, artifact_path),
            )
        elif packaging in ("exe", "json"):
            seen_imports[target_label] = True
            versioned_target_alias_label = "%s_extension" % to_repository_name(artifact["coordinates"])
            all_imports.append(
                "alias(\n\tname = \"%s\",\n\tactual = \"%s\",\n\tvisibility = [\"//visibility:public\"],\n)" % (target_label, versioned_target_alias_label),
            )
            if repository_ctx.attr.maven_install_json:
                all_imports.append(_genrule_copy_artifact_from_http_file(artifact, default_visibilities))
        elif target_label in labels_to_override:
            # Override target labels with the user provided mapping, instead of generating
            # a jvm_import/aar_import based on information in dep_tree.
            seen_imports[target_label] = True
            all_imports.append(
                "alias(\n\tname = \"%s\",\n\tactual = \"%s\",\n\tvisibility = [\"//visibility:public\"],)" % (target_label, labels_to_override.get(target_label)),
            )
            if repository_ctx.attr.maven_install_json:
                # Provide the downloaded artifact as a file target.
                all_imports.append(_genrule_copy_artifact_from_http_file(artifact, default_visibilities))
            raw_artifact = dict(artifact)
            raw_artifact["coordinates"] = "original_" + artifact["coordinates"]
            raw_artifact["maven_coordinates"] = artifact["coordinates"]
            raw_artifact["out"] = "original_" + artifact["file"]

            all_imports.extend(_generate_target(
                repository_ctx,
                jar_versionless_target_labels,
                explicit_artifacts,
                srcjar_paths,
                labels_to_override,
                repository_urls,
                neverlink_artifacts,
                testonly_artifacts,
                default_visibilities,
                raw_artifact,
            ))

        elif artifact_path != None and packaging != "pom":
            seen_imports[target_label] = True
            all_imports.extend(_generate_target(
                repository_ctx,
                jar_versionless_target_labels,
                explicit_artifacts,
                srcjar_paths,
                labels_to_override,
                repository_urls,
                neverlink_artifacts,
                testonly_artifacts,
                default_visibilities,
                artifact,
            ))
        else:  # artifact_path == None or packaging == "pom":
            # Special case for certain artifacts that only come with a POM file.
            # Such artifacts "aggregate" their dependencies, so they don't have
            # a JAR for download.
            #
            # Note that there are other possible reasons that the artifact_path is None:
            #
            # https://github.com/bazelbuild/rules_jvm_external/issues/70
            # https://github.com/bazelbuild/rules_jvm_external/issues/74
            #
            #
            # This can be due to the artifact being of a type that's unknown to
            # Coursier. This is increasingly rare as we add more types to
            # SUPPORTED_PACKAGING_TYPES. It's also increasingly
            # uncommon relatively to POM-only / parent artifacts. So when we
            # encounter an artifact without a filepath, we assume that it's a
            # parent artifact that just exports its dependencies, instead of
            # failing.
            seen_imports[target_label] = True
            target_import_string = ["java_library("]
            target_import_string.append("\tname = \"%s\"," % target_label)
            target_import_string.append("\texports = [")

            target_import_labels = []
            for dep in artifact.get("deps", []):
                dep_target_label = escape(strip_packaging_and_classifier_and_version(dep))

                # Coursier returns cyclic dependencies sometimes. Handle it here.
                # See https://github.com/bazelbuild/rules_jvm_external/issues/172
                if dep_target_label != target_label:
                    target_import_labels.append("\t\t\":%s\",\n" % dep_target_label)
            target_import_labels = _deduplicate_list(target_import_labels)

            target_import_string.append("".join(target_import_labels) + "\t],")
            coordinates = artifact.get("maven_coordinates", artifact["coordinates"])
            target_import_string.append("\ttags = [\"maven_coordinates=%s\"]," % coordinates)

            if repository_ctx.attr.strict_visibility and explicit_artifacts.get(simple_coord):
                target_import_string.append("\tvisibility = [\"//visibility:public\"],")
                alias_visibility = "\tvisibility = [\"//visibility:public\"],\n"
            else:
                target_import_string.append("\tvisibility = [%s]," % (",".join(["\"%s\"" % v for v in default_visibilities])))
                alias_visibility = "\tvisibility = [%s],\n" % (",".join(["\"%s\"" % v for v in default_visibilities]))

            target_import_string.append(")")

            all_imports.append("\n".join(target_import_string))

            versioned_target_alias_label = escape(strip_packaging_and_classifier(artifact["coordinates"]))
            all_imports.append("alias(\n\tname = \"%s\",\n\tactual = \"%s\",\n%s)" %
                               (versioned_target_alias_label, target_label, alias_visibility))

    return ("\n".join(all_imports), jar_versionless_target_labels)

parser = struct(
    generate_imports = _generate_imports,
)
