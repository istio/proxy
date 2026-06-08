load("@bazel_skylib//lib:unittest.bzl", "asserts", "unittest")
load("//private/extensions:maven.bzl", "process_gradle_versions_file")
load("//private/lib:toml_parser.bzl", "parse_toml")

def _simple_string_notation_impl(ctx):
    env = unittest.begin(ctx)

    toml_content = """\
[libraries]
commons-lang = "org.apache.commons:commons-lang3:3.12.0"
"""

    parsed = parse_toml(toml_content)
    artifacts, boms = process_gradle_versions_file(parsed, [])

    asserts.equals(env, 1, len(artifacts))
    asserts.equals(env, 0, len(boms))
    asserts.equals(env, "org.apache.commons", artifacts[0].group)
    asserts.equals(env, "commons-lang3", artifacts[0].artifact)
    asserts.equals(env, "3.12.0", artifacts[0].version)

    return unittest.end(env)

simple_string_notation_test = unittest.make(_simple_string_notation_impl)

def _map_with_module_and_inline_version_impl(ctx):
    env = unittest.begin(ctx)

    toml_content = """\
[libraries]
guava = { module = "com.google.guava:guava", version = "32.1.0-jre" }
"""

    parsed = parse_toml(toml_content)
    artifacts, boms = process_gradle_versions_file(parsed, [])

    asserts.equals(env, 1, len(artifacts))
    asserts.equals(env, 0, len(boms))
    asserts.equals(env, "com.google.guava", artifacts[0].group)
    asserts.equals(env, "guava", artifacts[0].artifact)
    asserts.equals(env, "32.1.0-jre", artifacts[0].version)

    return unittest.end(env)

map_with_module_and_inline_version_test = unittest.make(_map_with_module_and_inline_version_impl)

def _map_with_module_and_version_ref_impl(ctx):
    env = unittest.begin(ctx)

    toml_content = """\
[versions]
junit = "5.10.0"

[libraries]
junit-api = { module = "org.junit.jupiter:junit-jupiter-api", version.ref = "junit" }
"""

    parsed = parse_toml(toml_content)
    artifacts, boms = process_gradle_versions_file(parsed, [])

    asserts.equals(env, 1, len(artifacts))
    asserts.equals(env, 0, len(boms))
    asserts.equals(env, "org.junit.jupiter", artifacts[0].group)
    asserts.equals(env, "junit-jupiter-api", artifacts[0].artifact)
    asserts.equals(env, "5.10.0", artifacts[0].version)

    return unittest.end(env)

map_with_module_and_version_ref_test = unittest.make(_map_with_module_and_version_ref_impl)

def _map_with_module_no_version_impl(ctx):
    env = unittest.begin(ctx)

    toml_content = """\
[libraries]
guava-from-bom = { module = "com.google.guava:guava" }
"""

    parsed = parse_toml(toml_content)
    artifacts, boms = process_gradle_versions_file(parsed, [])

    asserts.equals(env, 1, len(artifacts))
    asserts.equals(env, 0, len(boms))
    asserts.equals(env, "com.google.guava", artifacts[0].group)
    asserts.equals(env, "guava", artifacts[0].artifact)
    asserts.equals(env, "", artifacts[0].version)

    return unittest.end(env)

map_with_module_no_version_test = unittest.make(_map_with_module_no_version_impl)

def _map_with_group_name_and_inline_version_impl(ctx):
    env = unittest.begin(ctx)

    toml_content = """\
[libraries]
androidx-core = { group = "androidx.core", name = "core-ktx", version = "1.12.0" }
"""

    parsed = parse_toml(toml_content)
    artifacts, boms = process_gradle_versions_file(parsed, [])

    asserts.equals(env, 1, len(artifacts))
    asserts.equals(env, 0, len(boms))
    asserts.equals(env, "androidx.core", artifacts[0].group)
    asserts.equals(env, "core-ktx", artifacts[0].artifact)
    asserts.equals(env, "1.12.0", artifacts[0].version)

    return unittest.end(env)

map_with_group_name_and_inline_version_test = unittest.make(_map_with_group_name_and_inline_version_impl)

def _map_with_group_name_and_version_ref_impl(ctx):
    env = unittest.begin(ctx)

    toml_content = """\
[versions]
kotlin = "1.9.0"

[libraries]
kotlin-stdlib = { group = "org.jetbrains.kotlin", name = "kotlin-stdlib", version.ref = "kotlin" }
"""

    parsed = parse_toml(toml_content)
    artifacts, boms = process_gradle_versions_file(parsed, [])

    asserts.equals(env, 1, len(artifacts))
    asserts.equals(env, 0, len(boms))
    asserts.equals(env, "org.jetbrains.kotlin", artifacts[0].group)
    asserts.equals(env, "kotlin-stdlib", artifacts[0].artifact)
    asserts.equals(env, "1.9.0", artifacts[0].version)

    return unittest.end(env)

map_with_group_name_and_version_ref_test = unittest.make(_map_with_group_name_and_version_ref_impl)

def _map_with_group_name_no_version_impl(ctx):
    env = unittest.begin(ctx)

    toml_content = """\
[versions]
compose = "1.5.0"

[libraries]
compose-ui = { group = "androidx.compose.ui", name = "ui", version.ref = "compose" }
"""

    parsed = parse_toml(toml_content)
    artifacts, boms = process_gradle_versions_file(parsed, [])

    asserts.equals(env, 1, len(artifacts))
    asserts.equals(env, 0, len(boms))
    asserts.equals(env, "androidx.compose.ui", artifacts[0].group)
    asserts.equals(env, "ui", artifacts[0].artifact)
    asserts.equals(env, "1.5.0", artifacts[0].version)

    return unittest.end(env)

map_with_group_name_no_version_test = unittest.make(_map_with_group_name_no_version_impl)

def _map_with_module_and_packaging_impl(ctx):
    env = unittest.begin(ctx)

    toml_content = """\
[libraries]
play-services = { module = "com.google.android.gms:play-services-tasks", package = "aar", version = "18.1.0" }
"""

    parsed = parse_toml(toml_content)
    artifacts, boms = process_gradle_versions_file(parsed, [])

    asserts.equals(env, 1, len(artifacts))
    asserts.equals(env, 0, len(boms))
    asserts.equals(env, "com.google.android.gms", artifacts[0].group)
    asserts.equals(env, "play-services-tasks", artifacts[0].artifact)
    asserts.equals(env, "18.1.0", artifacts[0].version)
    asserts.equals(env, "aar", artifacts[0].packaging)

    return unittest.end(env)

map_with_module_and_packaging_test = unittest.make(_map_with_module_and_packaging_impl)

def _map_with_group_name_and_packaging_impl(ctx):
    env = unittest.begin(ctx)

    toml_content = """\
[libraries]
android-material = { group = "com.google.android.material", name = "material", version = "1.10.0", package = "aar" }
"""

    parsed = parse_toml(toml_content)
    artifacts, boms = process_gradle_versions_file(parsed, [])

    asserts.equals(env, 1, len(artifacts))
    asserts.equals(env, 0, len(boms))
    asserts.equals(env, "com.google.android.material", artifacts[0].group)
    asserts.equals(env, "material", artifacts[0].artifact)
    asserts.equals(env, "1.10.0", artifacts[0].version)
    asserts.equals(env, "aar", artifacts[0].packaging)

    return unittest.end(env)

map_with_group_name_and_packaging_test = unittest.make(_map_with_group_name_and_packaging_impl)

def _bom_handling_impl(ctx):
    env = unittest.begin(ctx)

    toml_content = """\
[libraries]
guava-bom = { module = "com.google.guava:guava-bom", version = "32.1.0-jre" }
guava = { module = "com.google.guava:guava", version = "32.1.0-jre" }
"""

    parsed = parse_toml(toml_content)
    artifacts, boms = process_gradle_versions_file(parsed, ["com.google.guava:guava-bom"])

    asserts.equals(env, 1, len(artifacts))
    asserts.equals(env, 1, len(boms))

    # Check the artifact
    asserts.equals(env, "com.google.guava", artifacts[0].group)
    asserts.equals(env, "guava", artifacts[0].artifact)
    asserts.equals(env, "32.1.0-jre", artifacts[0].version)

    # Check the BOM
    asserts.equals(env, "com.google.guava", boms[0].group)
    asserts.equals(env, "guava-bom", boms[0].artifact)
    asserts.equals(env, "32.1.0-jre", boms[0].version)

    return unittest.end(env)

bom_handling_test = unittest.make(_bom_handling_impl)

def _multiple_libraries_impl(ctx):
    env = unittest.begin(ctx)

    toml_content = """\
[versions]
kotlin = "1.9.0"
junit = "5.10.0"

[libraries]
commons-lang = "org.apache.commons:commons-lang3:3.12.0"
guava = { module = "com.google.guava:guava", version = "32.1.0-jre" }
junit-api = { module = "org.junit.jupiter:junit-jupiter-api", version.ref = "junit" }
kotlin-stdlib = { group = "org.jetbrains.kotlin", name = "kotlin-stdlib", version.ref = "kotlin" }
"""

    parsed = parse_toml(toml_content)
    artifacts, boms = process_gradle_versions_file(parsed, [])

    asserts.equals(env, 4, len(artifacts))
    asserts.equals(env, 0, len(boms))

    return unittest.end(env)

multiple_libraries_test = unittest.make(_multiple_libraries_impl)

def version_catalogs_test_suite():
    unittest.suite(
        "version_catalogs_tests",
        simple_string_notation_test,
        map_with_module_and_inline_version_test,
        map_with_module_and_version_ref_test,
        map_with_module_no_version_test,
        map_with_group_name_and_inline_version_test,
        map_with_group_name_and_version_ref_test,
        map_with_group_name_no_version_test,
        map_with_module_and_packaging_test,
        map_with_group_name_and_packaging_test,
        bom_handling_test,
        multiple_libraries_test,
    )
