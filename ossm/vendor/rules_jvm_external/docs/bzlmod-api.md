<!-- Generated with Stardoc: http://skydoc.bazel.build -->



<a id="maven"></a>

## maven

<pre>
maven = use_extension("@rules_jvm_external//:extensions.bzl", "maven")
maven.amend_artifact(<a href="#maven.amend_artifact-name">name</a>, <a href="#maven.amend_artifact-coordinates">coordinates</a>, <a href="#maven.amend_artifact-exclusions">exclusions</a>, <a href="#maven.amend_artifact-force_version">force_version</a>, <a href="#maven.amend_artifact-neverlink">neverlink</a>, <a href="#maven.amend_artifact-testonly">testonly</a>)
maven.artifact(<a href="#maven.artifact-name">name</a>, <a href="#maven.artifact-artifact">artifact</a>, <a href="#maven.artifact-classifier">classifier</a>, <a href="#maven.artifact-exclusions">exclusions</a>, <a href="#maven.artifact-force_version">force_version</a>, <a href="#maven.artifact-group">group</a>, <a href="#maven.artifact-neverlink">neverlink</a>, <a href="#maven.artifact-packaging">packaging</a>,
               <a href="#maven.artifact-testonly">testonly</a>, <a href="#maven.artifact-version">version</a>)
maven.from_toml(<a href="#maven.from_toml-name">name</a>, <a href="#maven.from_toml-bom_modules">bom_modules</a>, <a href="#maven.from_toml-libs_versions_toml">libs_versions_toml</a>)
maven.install(<a href="#maven.install-name">name</a>, <a href="#maven.install-aar_import_bzl_label">aar_import_bzl_label</a>, <a href="#maven.install-additional_coursier_options">additional_coursier_options</a>, <a href="#maven.install-additional_netrc_lines">additional_netrc_lines</a>,
              <a href="#maven.install-artifacts">artifacts</a>, <a href="#maven.install-boms">boms</a>, <a href="#maven.install-duplicate_version_warning">duplicate_version_warning</a>, <a href="#maven.install-excluded_artifacts">excluded_artifacts</a>, <a href="#maven.install-exclusions">exclusions</a>,
              <a href="#maven.install-fail_if_repin_required">fail_if_repin_required</a>, <a href="#maven.install-fail_on_missing_checksum">fail_on_missing_checksum</a>, <a href="#maven.install-fetch_javadoc">fetch_javadoc</a>, <a href="#maven.install-fetch_sources">fetch_sources</a>,
              <a href="#maven.install-generate_compat_repositories">generate_compat_repositories</a>, <a href="#maven.install-ignore_empty_files">ignore_empty_files</a>, <a href="#maven.install-known_contributing_modules">known_contributing_modules</a>, <a href="#maven.install-lock_file">lock_file</a>,
              <a href="#maven.install-repin_instructions">repin_instructions</a>, <a href="#maven.install-repositories">repositories</a>, <a href="#maven.install-resolve_timeout">resolve_timeout</a>, <a href="#maven.install-resolver">resolver</a>, <a href="#maven.install-strict_visibility">strict_visibility</a>,
              <a href="#maven.install-strict_visibility_value">strict_visibility_value</a>, <a href="#maven.install-use_credentials_from_home_netrc_file">use_credentials_from_home_netrc_file</a>,
              <a href="#maven.install-use_starlark_android_rules">use_starlark_android_rules</a>, <a href="#maven.install-version_conflict_policy">version_conflict_policy</a>)
maven.override(<a href="#maven.override-name">name</a>, <a href="#maven.override-coordinates">coordinates</a>, <a href="#maven.override-target">target</a>)
</pre>


**TAG CLASSES**

<a id="maven.amend_artifact"></a>

### amend_artifact

Modifies an artifact with `coordinates` defined in other tags with additional properties.

**Attributes**

| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="maven.amend_artifact-name"></a>name |  -   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | optional |  `"maven"`  |
| <a id="maven.amend_artifact-coordinates"></a>coordinates |  Coordinates of the artifact to amend. Only `group:artifact` are used for matching.   | String | required |  |
| <a id="maven.amend_artifact-exclusions"></a>exclusions |  Maven artifact tuples, in `artifactId:groupId` format   | List of strings | optional |  `[]`  |
| <a id="maven.amend_artifact-force_version"></a>force_version |  -   | Boolean | optional |  `False`  |
| <a id="maven.amend_artifact-neverlink"></a>neverlink |  -   | Boolean | optional |  `False`  |
| <a id="maven.amend_artifact-testonly"></a>testonly |  -   | Boolean | optional |  `False`  |

<a id="maven.artifact"></a>

### artifact

Used to define a single artifact where the simple coordinates are insufficient. Will be added to the other artifacts declared by tags with the same `name` attribute.

**Attributes**

| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="maven.artifact-name"></a>name |  -   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | optional |  `"maven"`  |
| <a id="maven.artifact-artifact"></a>artifact |  -   | String | required |  |
| <a id="maven.artifact-classifier"></a>classifier |  -   | String | optional |  `""`  |
| <a id="maven.artifact-exclusions"></a>exclusions |  Maven artifact tuples, in `artifactId:groupId` format   | List of strings | optional |  `[]`  |
| <a id="maven.artifact-force_version"></a>force_version |  -   | Boolean | optional |  `False`  |
| <a id="maven.artifact-group"></a>group |  -   | String | required |  |
| <a id="maven.artifact-neverlink"></a>neverlink |  -   | Boolean | optional |  `False`  |
| <a id="maven.artifact-packaging"></a>packaging |  -   | String | optional |  `""`  |
| <a id="maven.artifact-testonly"></a>testonly |  -   | Boolean | optional |  `False`  |
| <a id="maven.artifact-version"></a>version |  -   | String | optional |  `""`  |

<a id="maven.from_toml"></a>

### from_toml

Allows a project to import dependencies from a Gradle format `libs.versions.toml` file.

**Attributes**

| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="maven.from_toml-name"></a>name |  -   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | optional |  `"maven"`  |
| <a id="maven.from_toml-bom_modules"></a>bom_modules |  List of modules in `group:artifact` format to treat as BOMs, not artifacts   | List of strings | optional |  `[]`  |
| <a id="maven.from_toml-libs_versions_toml"></a>libs_versions_toml |  Gradle `libs.versions.toml` file to use   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |

<a id="maven.install"></a>

### install

Combines artifact and bom declarations with setting the location of lock files to use, and repositories to download artifacts from. There can only be one `install` tag with a given `name` per module. `install` tags with the same name across multiple modules will be merged, with the root module taking precedence.

**Attributes**

| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="maven.install-name"></a>name |  -   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | optional |  `"maven"`  |
| <a id="maven.install-aar_import_bzl_label"></a>aar_import_bzl_label |  The label (as a string) to use to import aar_import from   | String | optional |  `"@build_bazel_rules_android//android:rules.bzl"`  |
| <a id="maven.install-additional_coursier_options"></a>additional_coursier_options |  Additional options that will be passed to coursier.   | List of strings | optional |  `[]`  |
| <a id="maven.install-additional_netrc_lines"></a>additional_netrc_lines |  Additional lines prepended to the netrc file used by `http_file` (with `maven_install_json` only).   | List of strings | optional |  `[]`  |
| <a id="maven.install-artifacts"></a>artifacts |  Maven artifact tuples, in `artifactId:groupId:version` format   | List of strings | optional |  `[]`  |
| <a id="maven.install-boms"></a>boms |  Maven BOM tuples, in `artifactId:groupId:version` format   | List of strings | optional |  `[]`  |
| <a id="maven.install-duplicate_version_warning"></a>duplicate_version_warning |  What to do if there are duplicate artifacts<br><br>If "error", then print a message and fail the build. If "warn", then print a warning and continue. If "none", then do nothing.   | String | optional |  `"warn"`  |
| <a id="maven.install-excluded_artifacts"></a>excluded_artifacts |  Artifacts to exclude, in `artifactId:groupId` format. Only used on unpinned installs   | List of strings | optional |  `[]`  |
| <a id="maven.install-exclusions"></a>exclusions |  Maven artifact tuples, in `artifactId:groupId` format   | List of strings | optional |  `[]`  |
| <a id="maven.install-fail_if_repin_required"></a>fail_if_repin_required |  Whether to fail the build if the maven_artifact inputs have changed but the lock file has not been repinned.   | Boolean | optional |  `True`  |
| <a id="maven.install-fail_on_missing_checksum"></a>fail_on_missing_checksum |  -   | Boolean | optional |  `True`  |
| <a id="maven.install-fetch_javadoc"></a>fetch_javadoc |  -   | Boolean | optional |  `False`  |
| <a id="maven.install-fetch_sources"></a>fetch_sources |  -   | Boolean | optional |  `False`  |
| <a id="maven.install-generate_compat_repositories"></a>generate_compat_repositories |  Additionally generate repository aliases in a .bzl file for all JAR artifacts. For example, `@maven//:com_google_guava_guava` can also be referenced as `@com_google_guava_guava//jar`.   | Boolean | optional |  `False`  |
| <a id="maven.install-ignore_empty_files"></a>ignore_empty_files |  Treat jars that are empty as if they were not found.   | Boolean | optional |  `False`  |
| <a id="maven.install-known_contributing_modules"></a>known_contributing_modules |  List of Bzlmod modules that are known to be contributing to this repository. Only honoured for the root module.   | List of strings | optional |  `[]`  |
| <a id="maven.install-lock_file"></a>lock_file |  -   | <a href="https://bazel.build/concepts/labels">Label</a> | optional |  `None`  |
| <a id="maven.install-repin_instructions"></a>repin_instructions |  Instructions to re-pin the repository if required. Many people have wrapper scripts for keeping dependencies up to date, and would like to point users to that instead of the default. Only honoured for the root module.   | String | optional |  `""`  |
| <a id="maven.install-repositories"></a>repositories |  -   | List of strings | optional |  `["https://repo1.maven.org/maven2"]`  |
| <a id="maven.install-resolve_timeout"></a>resolve_timeout |  -   | Integer | optional |  `600`  |
| <a id="maven.install-resolver"></a>resolver |  The resolver to use. Only honoured for the root module.   | String | optional |  `"coursier"`  |
| <a id="maven.install-strict_visibility"></a>strict_visibility |  Controls visibility of transitive dependencies.<br><br>If "True", transitive dependencies are private and invisible to user's rules. If "False", transitive dependencies are public and visible to user's rules.   | Boolean | optional |  `False`  |
| <a id="maven.install-strict_visibility_value"></a>strict_visibility_value |  -   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `["@rules_jvm_external//visibility:private"]`  |
| <a id="maven.install-use_credentials_from_home_netrc_file"></a>use_credentials_from_home_netrc_file |  Whether to pass machine login credentials from the ~/.netrc file to coursier.   | Boolean | optional |  `False`  |
| <a id="maven.install-use_starlark_android_rules"></a>use_starlark_android_rules |  Whether to use the native or Starlark version of the Android rules.   | Boolean | optional |  `False`  |
| <a id="maven.install-version_conflict_policy"></a>version_conflict_policy |  Policy for user-defined vs. transitive dependency version conflicts<br><br>If "pinned", choose the user-specified version in maven_install unconditionally. If "default", follow Coursier's default policy.   | String | optional |  `"default"`  |

<a id="maven.override"></a>

### override

Allows specific maven coordinates to be redirected elsewhere. Commonly used to replace an external dependency with another, or a compatible implementation from within this module.

**Attributes**

| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="maven.override-name"></a>name |  -   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | optional |  `"maven"`  |
| <a id="maven.override-coordinates"></a>coordinates |  Maven artifact tuple in `artifactId:groupId` format   | String | required |  |
| <a id="maven.override-target"></a>target |  Target to use in place of maven coordinates   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |


