# Basic functions

These are the basic functions to get started.

To use these functions, load them at the top of your BUILD file. For example:

```python
load("@rules_jvm_external//:defs.bzl", "maven_install", "artifact")
```
<!-- Generated with Stardoc: http://skydoc.bazel.build -->



<a id="javadoc"></a>

## javadoc

<pre>
load("@rules_jvm_external//:defs.bzl", "javadoc")

javadoc(<a href="#javadoc-name">name</a>, <a href="#javadoc-deps">deps</a>, <a href="#javadoc-additional_dependencies">additional_dependencies</a>, <a href="#javadoc-doc_deps">doc_deps</a>, <a href="#javadoc-doc_resources">doc_resources</a>, <a href="#javadoc-doc_url">doc_url</a>, <a href="#javadoc-excluded_packages">excluded_packages</a>,
        <a href="#javadoc-excluded_workspaces">excluded_workspaces</a>, <a href="#javadoc-included_packages">included_packages</a>, <a href="#javadoc-javadocopts">javadocopts</a>)
</pre>

Generate a javadoc from all the `deps`

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="javadoc-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="javadoc-deps"></a>deps |  The java libraries to generate javadocs for.<br><br>The source jars of each dep will be used to generate the javadocs. Currently docs for transitive dependencies are not generated.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | required |  |
| <a id="javadoc-additional_dependencies"></a>additional_dependencies |  Mapping of `Label`s to the excluded workspace names. Note that this must match the values passed to the `pom_file` rule so the `pom.xml` correctly lists these dependencies.   | <a href="https://bazel.build/rules/lib/dict">Dictionary: Label -> String</a> | optional |  `{}`  |
| <a id="javadoc-doc_deps"></a>doc_deps |  `javadoc` targets referenced by the current target.<br><br>Use this to automatically add appropriate `-linkoffline` javadoc options to resolve references to packages documented by the given javadoc targets that have `url` specified.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="javadoc-doc_resources"></a>doc_resources |  Resources to include in the javadoc jar.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional |  `[]`  |
| <a id="javadoc-doc_url"></a>doc_url |  The URL at which this documentation will be hosted.<br><br>This information is only used by javadoc targets depending on this target.   | String | optional |  `""`  |
| <a id="javadoc-excluded_packages"></a>excluded_packages |  A list of packages to exclude from the generated javadoc. Wildcards are supported at the end of the package name. For example, `com.example.*` will exclude all the subpackages of `com.example`, while `com.example` will exclude only the files directly in `com.example`.   | List of strings | optional |  `[]`  |
| <a id="javadoc-excluded_workspaces"></a>excluded_workspaces |  A list of bazel workspace names to exclude from the generated jar   | List of strings | optional |  `["com_google_protobuf", "protobuf"]`  |
| <a id="javadoc-included_packages"></a>included_packages |  A list of packages to include in the generated javadoc. Wildcards are supported at the end of the package name. For example, `com.example.*` will include all the subpackages of `com.example`, while `com.example` will include only the files directly in `com.example`.   | List of strings | optional |  `[]`  |
| <a id="javadoc-javadocopts"></a>javadocopts |  javadoc options. Note sources and classpath are derived from the deps. Any additional options can be passed here. If nothing is passed, a default list of options is used: ["-notimestamp", "-use", "-quiet", "-Xdoclint:-missing", "-encoding", "UTF8"]   | List of strings | optional |  `["-notimestamp", "-use", "-quiet", "-Xdoclint:-missing", "-encoding", "UTF8"]`  |


<a id="java_export"></a>

## java_export

<pre>
load("@rules_jvm_external//:defs.bzl", "java_export")

java_export(<a href="#java_export-name">name</a>, <a href="#java_export-maven_coordinates">maven_coordinates</a>, <a href="#java_export-manifest_entries">manifest_entries</a>, <a href="#java_export-deploy_env">deploy_env</a>, <a href="#java_export-excluded_workspaces">excluded_workspaces</a>, <a href="#java_export-exclusions">exclusions</a>,
            <a href="#java_export-pom_template">pom_template</a>, <a href="#java_export-allowed_duplicate_names">allowed_duplicate_names</a>, <a href="#java_export-visibility">visibility</a>, <a href="#java_export-tags">tags</a>, <a href="#java_export-testonly">testonly</a>, <a href="#java_export-classifier_artifacts">classifier_artifacts</a>,
            <a href="#java_export-publish_maven_metadata">publish_maven_metadata</a>, <a href="#java_export-kwargs">kwargs</a>)
</pre>

Extends `java_library` to allow maven artifacts to be uploaded.

This macro can be used as a drop-in replacement for `java_library`, but
also generates an implicit `name.publish` target that can be run to publish
maven artifacts derived from this macro to a maven repository. The publish
rule understands the following variables (declared using `--define` when
using `bazel run`, or as environment variables in ALL_CAPS form):

  * `maven_repo`: A URL for the repo to use. May be "https" or "file". Can also be set with environment variable `MAVEN_REPO`.
  * `maven_user`: The user name to use when uploading to the maven repository. Can also be set with environment variable `MAVEN_USER`.
  * `maven_password`: The password to use when uploading to the maven repository. Can also be set with environment variable `MAVEN_PASSWORD`.


This macro also generates a `name-pom` target that creates the `pom.xml` file
associated with the artifacts. The template used is derived from the (optional)
`pom_template` argument, and the following substitutions are performed on
the template file:

  * `{groupId}`: Replaced with the maven coordinates group ID.
  * `{artifactId}`: Replaced with the maven coordinates artifact ID.
  * `{version}`: Replaced by the maven coordinates version.
  * `{type}`: Replaced by the maven coordinates type, if present (defaults to "jar")
  * `{scope}`: Replaced by the maven coordinates type, if present (defaults to "compile")
  * `{dependencies}`: Replaced by a list of maven dependencies directly relied upon
    by java_library targets within the artifact.

The "edges" of the artifact are found by scanning targets that contribute to
runtime dependencies for the following tags:

  * `maven_coordinates=group:artifact:type:version`: Specifies a dependency of
    this artifact.
  * `maven:compile-only`: Specifies that this dependency should not be listed
    as a dependency of the artifact being generated.

To skip generation of the javadoc jar, add the `no-javadocs` tag to the target.

Generated rules:
  * `name`: A `java_library` that other rules can depend upon.
  * `name-docs`: A javadoc jar file.
  * `name-pom`: The pom.xml file.
  * `name.publish`: To be executed by `bazel run` to publish to a maven repo.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="java_export-name"></a>name |  A unique name for this target   |  none |
| <a id="java_export-maven_coordinates"></a>maven_coordinates |  The maven coordinates for this target.   |  none |
| <a id="java_export-manifest_entries"></a>manifest_entries |  A dict of `String: String` containing additional manifest entry attributes and values.   |  `{}` |
| <a id="java_export-deploy_env"></a>deploy_env |  A list of labels of Java targets to exclude from the generated jar. [`java_binary`](https://bazel.build/reference/be/java#java_binary) targets are *not* supported.   |  `[]` |
| <a id="java_export-excluded_workspaces"></a>excluded_workspaces |  A dict of strings representing the workspace names of artifacts that should not be included in the maven jar to a `Label` pointing to the dependency that workspace should be replaced by, or `None` if the exclusion shouldn't be replaced with an extra dependency.   |  `{"com_google_protobuf": None, "protobuf": None}` |
| <a id="java_export-exclusions"></a>exclusions |  Mapping of target labels to a list of exclusions to be added to the POM file. Each label must correspond to a direct maven dependency of this target. Each exclusion is represented as a `group:artifact` string.   |  `{}` |
| <a id="java_export-pom_template"></a>pom_template |  The template to be used for the pom.xml file.   |  `None` |
| <a id="java_export-allowed_duplicate_names"></a>allowed_duplicate_names |  A list of `String` containing patterns for files that can be included more than once in the jar file. Examples include `["log4j.properties"]`   |  `None` |
| <a id="java_export-visibility"></a>visibility |  The visibility of the target   |  `None` |
| <a id="java_export-tags"></a>tags |  <p align="center"> - </p>   |  `[]` |
| <a id="java_export-testonly"></a>testonly |  <p align="center"> - </p>   |  `None` |
| <a id="java_export-classifier_artifacts"></a>classifier_artifacts |  A dict of classifier -> artifact of additional artifacts to publish to Maven.   |  `{}` |
| <a id="java_export-publish_maven_metadata"></a>publish_maven_metadata |  Whether to publish a maven-metadata.xml to remote repository. Some repositories (like AWS CodeArtifact) require the client to publish this file. It is disabled by default.   |  `False` |
| <a id="java_export-kwargs"></a>kwargs |  <p align="center"> - </p>   |  none |


<a id="maven_bom"></a>

## maven_bom

<pre>
load("@rules_jvm_external//:defs.bzl", "maven_bom")

maven_bom(<a href="#maven_bom-name">name</a>, <a href="#maven_bom-maven_coordinates">maven_coordinates</a>, <a href="#maven_bom-java_exports">java_exports</a>, <a href="#maven_bom-bom_pom_template">bom_pom_template</a>, <a href="#maven_bom-dependencies_maven_coordinates">dependencies_maven_coordinates</a>,
          <a href="#maven_bom-dependencies_pom_template">dependencies_pom_template</a>, <a href="#maven_bom-tags">tags</a>, <a href="#maven_bom-testonly">testonly</a>, <a href="#maven_bom-visibility">visibility</a>, <a href="#maven_bom-toolchains">toolchains</a>)
</pre>

Generates a Maven BOM `pom.xml` file and an optional "dependencies" `pom.xml`.

The generated BOM will contain a list of all the coordinates of the
`java_export` targets in the `java_exports` parameters. An optional
dependencies artifact will be created if the parameter
`dependencies_maven_coordinates` is set.

Both the BOM and dependencies artifact can be templatised to support
customisation, but a sensible default template will be used if none is
provided. The template used is derived from the (optional)
`pom_template` argument, and the following substitutions are performed on
the template file:

  * `{groupId}`: Replaced with the maven coordinates group ID.
  * `{artifactId}`: Replaced with the maven coordinates artifact ID.
  * `{version}`: Replaced by the maven coordinates version.
  * `{dependencies}`: Replaced by a list of maven dependencies directly relied upon
    by java_library targets within the artifact.

To publish, call the implicit `*.publish` target(s).

The maven repository may accessed locally using a `file://` URL, or
remotely using an `https://` URL. The following flags may be set
using `--define`:

  * `gpg_sign`: Whether to sign artifacts using GPG
  * `maven_repo`: A URL for the repo to use. May be "https" or "file".
  * `maven_user`: The user name to use when uploading to the maven repository.
  * `maven_password`: The password to use when uploading to the maven repository.

When signing with GPG, the current default key is used.

Generated rules:
  * `name`: The BOM file itself.
  * `name.publish`: To be executed by `bazel run` to publish the BOM to a maven repo
  * `name-dependencies`: The BOM file for the dependencies `pom.xml`. Only generated if `dependencies_maven_coordinates` is set.
  * `name-dependencies.publish`: To be executed by `bazel run` to publish the dependencies `pom.xml` to a maven rpo. Only generated if `dependencies_maven_coordinates` is set.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="maven_bom-name"></a>name |  A unique name for this rule.   |  none |
| <a id="maven_bom-maven_coordinates"></a>maven_coordinates |  The maven coordinates of this BOM in `groupId:artifactId:version` form.   |  none |
| <a id="maven_bom-java_exports"></a>java_exports |  A list of `java_export` targets that are used to generate the BOM.   |  none |
| <a id="maven_bom-bom_pom_template"></a>bom_pom_template |  A template used for generating the `pom.xml` of the BOM at `maven_coordinates` (optional)   |  `None` |
| <a id="maven_bom-dependencies_maven_coordinates"></a>dependencies_maven_coordinates |  The maven coordinates of a dependencies artifact to generate in GAV format. If empty, none will be generated. (optional)   |  `None` |
| <a id="maven_bom-dependencies_pom_template"></a>dependencies_pom_template |  A template used for generating the `pom.xml` of the dependencies artifact at `dependencies_maven_coordinates` (optional)   |  `None` |
| <a id="maven_bom-tags"></a>tags |  <p align="center"> - </p>   |  `None` |
| <a id="maven_bom-testonly"></a>testonly |  <p align="center"> - </p>   |  `None` |
| <a id="maven_bom-visibility"></a>visibility |  <p align="center"> - </p>   |  `None` |
| <a id="maven_bom-toolchains"></a>toolchains |  <p align="center"> - </p>   |  `[]` |


<a id="maven_install"></a>

## maven_install

<pre>
load("@rules_jvm_external//:defs.bzl", "maven_install")

maven_install(<a href="#maven_install-name">name</a>, <a href="#maven_install-repositories">repositories</a>, <a href="#maven_install-artifacts">artifacts</a>, <a href="#maven_install-boms">boms</a>, <a href="#maven_install-resolver">resolver</a>, <a href="#maven_install-fail_on_missing_checksum">fail_on_missing_checksum</a>,
              <a href="#maven_install-fetch_sources">fetch_sources</a>, <a href="#maven_install-fetch_javadoc">fetch_javadoc</a>, <a href="#maven_install-excluded_artifacts">excluded_artifacts</a>, <a href="#maven_install-generate_compat_repositories">generate_compat_repositories</a>,
              <a href="#maven_install-version_conflict_policy">version_conflict_policy</a>, <a href="#maven_install-maven_install_json">maven_install_json</a>, <a href="#maven_install-override_targets">override_targets</a>, <a href="#maven_install-strict_visibility">strict_visibility</a>,
              <a href="#maven_install-strict_visibility_value">strict_visibility_value</a>, <a href="#maven_install-resolve_timeout">resolve_timeout</a>, <a href="#maven_install-additional_netrc_lines">additional_netrc_lines</a>,
              <a href="#maven_install-use_credentials_from_home_netrc_file">use_credentials_from_home_netrc_file</a>, <a href="#maven_install-fail_if_repin_required">fail_if_repin_required</a>,
              <a href="#maven_install-use_starlark_android_rules">use_starlark_android_rules</a>, <a href="#maven_install-aar_import_bzl_label">aar_import_bzl_label</a>, <a href="#maven_install-duplicate_version_warning">duplicate_version_warning</a>,
              <a href="#maven_install-repin_instructions">repin_instructions</a>, <a href="#maven_install-ignore_empty_files">ignore_empty_files</a>, <a href="#maven_install-additional_coursier_options">additional_coursier_options</a>)
</pre>

Resolves and fetches artifacts transitively from Maven repositories.

This macro runs a repository rule that invokes the Coursier CLI to resolve
and fetch Maven artifacts transitively.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="maven_install-name"></a>name |  A unique name for this Bazel external repository.   |  `"maven"` |
| <a id="maven_install-repositories"></a>repositories |  A list of Maven repository URLs, specified in lookup order.<br><br>Supports URLs with HTTP Basic Authentication, e.g. "https://username:password@example.com".   |  `[]` |
| <a id="maven_install-artifacts"></a>artifacts |  A list of Maven artifact coordinates in the form of `group:artifact:version`.   |  `[]` |
| <a id="maven_install-boms"></a>boms |  A list of Maven artifact coordinates in the form of `group:artifact:version` which refer to Maven BOMs.   |  `[]` |
| <a id="maven_install-resolver"></a>resolver |  Which resolver to use. One of `coursier`, `gradle` or `maven`.   |  `"coursier"` |
| <a id="maven_install-fail_on_missing_checksum"></a>fail_on_missing_checksum |  fail the fetch if checksum attributes are not present.   |  `True` |
| <a id="maven_install-fetch_sources"></a>fetch_sources |  Additionally fetch source JARs.   |  `False` |
| <a id="maven_install-fetch_javadoc"></a>fetch_javadoc |  Additionally fetch javadoc JARs.   |  `False` |
| <a id="maven_install-excluded_artifacts"></a>excluded_artifacts |  A list of Maven artifact coordinates in the form of `group:artifact` to be excluded from the transitive dependencies.   |  `[]` |
| <a id="maven_install-generate_compat_repositories"></a>generate_compat_repositories |  Additionally generate repository aliases in a .bzl file for all JAR artifacts. For example, `@maven//:com_google_guava_guava` can also be referenced as `@com_google_guava_guava//jar`.   |  `False` |
| <a id="maven_install-version_conflict_policy"></a>version_conflict_policy |  Policy for user-defined vs. transitive dependency version conflicts.  If "pinned", choose the user's version unconditionally.  If "default", follow Coursier's default policy.   |  `"default"` |
| <a id="maven_install-maven_install_json"></a>maven_install_json |  A label to a `maven_install.json` file to use pinned artifacts for generating build targets. e.g `//:maven_install.json`.   |  `None` |
| <a id="maven_install-override_targets"></a>override_targets |  A mapping of `group:artifact` to Bazel target labels. All occurrences of the target label for `group:artifact` will be an alias to the specified label, therefore overriding the original generated `jvm_import` or `aar_import` target.   |  `{}` |
| <a id="maven_install-strict_visibility"></a>strict_visibility |  Controls visibility of transitive dependencies. If `True`, transitive dependencies are private and invisible to user's rules. If `False`, transitive dependencies are public and visible to user's rules.   |  `False` |
| <a id="maven_install-strict_visibility_value"></a>strict_visibility_value |  Allows changing transitive dependencies strict visibility scope from private to specified scopes list.   |  `["//visibility:private"]` |
| <a id="maven_install-resolve_timeout"></a>resolve_timeout |  The execution timeout of resolving and fetching artifacts.   |  `600` |
| <a id="maven_install-additional_netrc_lines"></a>additional_netrc_lines |  Additional lines prepended to the netrc file used by `http_file` (with `maven_install_json` only).   |  `[]` |
| <a id="maven_install-use_credentials_from_home_netrc_file"></a>use_credentials_from_home_netrc_file |  Whether to pass machine login credentials from the ~/.netrc file to coursier.   |  `False` |
| <a id="maven_install-fail_if_repin_required"></a>fail_if_repin_required |  Whether to fail the build if the required maven artifacts have been changed but not repinned. Requires the `maven_install_json` to have been set.   |  `True` |
| <a id="maven_install-use_starlark_android_rules"></a>use_starlark_android_rules |  Whether to use the native or Starlark version of the Android rules. Default is False if the running version of Bazel supports native aar_import. If the running version of Bazel does not support native aar_import, this parameter is ignored and the Starlark Android rules is used.   |  `False` |
| <a id="maven_install-aar_import_bzl_label"></a>aar_import_bzl_label |  The label (as a string) to use to import aar_import from. This is usually needed only if the top-level workspace file does not use the typical default repository name to import the Android Starlark rules. Default is "@rules_android//rules:rules.bzl".   |  `"@rules_android//rules:rules.bzl"` |
| <a id="maven_install-duplicate_version_warning"></a>duplicate_version_warning |  What to do if an artifact is specified multiple times. If "error" then fail the build, if "warn" then print a message and continue, if "none" then do nothing. The default is "warn".   |  `"warn"` |
| <a id="maven_install-repin_instructions"></a>repin_instructions |  Instructions to re-pin dependencies in your repository. Will be shown when re-pinning is required.   |  `None` |
| <a id="maven_install-ignore_empty_files"></a>ignore_empty_files |  Treat jars that are empty as if they were not found.   |  `False` |
| <a id="maven_install-additional_coursier_options"></a>additional_coursier_options |  Additional options that will be passed to coursier.   |  `[]` |


# Maven specification functions

These are helper functions to specify more information about Maven artifacts and
repositories in `maven_install`.

To use these functions, load the `maven` struct at the top of your BUILD file:

```python
load("@rules_jvm_external//:specs.bzl", "maven")
```
<!-- Generated with Stardoc: http://skydoc.bazel.build -->



<a id="maven.artifact"></a>

## maven.artifact

<pre>
load("@rules_jvm_external//:specs.bzl", "maven")

maven.artifact(<a href="#maven.artifact-group">group</a>, <a href="#maven.artifact-artifact">artifact</a>, <a href="#maven.artifact-version">version</a>, <a href="#maven.artifact-packaging">packaging</a>, <a href="#maven.artifact-classifier">classifier</a>, <a href="#maven.artifact-override_license_types">override_license_types</a>, <a href="#maven.artifact-exclusions">exclusions</a>,
               <a href="#maven.artifact-neverlink">neverlink</a>, <a href="#maven.artifact-testonly">testonly</a>, <a href="#maven.artifact-force_version">force_version</a>)
</pre>

Generates the data map for a Maven artifact given the available information about its coordinates.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="maven.artifact-group"></a>group |  The Maven artifact coordinate group name (ex: "com.google.guava").   |  none |
| <a id="maven.artifact-artifact"></a>artifact |  The Maven artifact coordinate artifact name (ex: "guava").   |  none |
| <a id="maven.artifact-version"></a>version |  The Maven artifact coordinate version name (ex: "27.0-jre").   |  `""` |
| <a id="maven.artifact-packaging"></a>packaging |  The Maven packaging specifier (ex: "jar").   |  `None` |
| <a id="maven.artifact-classifier"></a>classifier |  The Maven artifact classifier (ex: "javadoc").   |  `None` |
| <a id="maven.artifact-override_license_types"></a>override_license_types |  An array of Bazel license type strings to use for this artifact's rules (overrides autodetection) (ex: ["notify"]).   |  `None` |
| <a id="maven.artifact-exclusions"></a>exclusions |  An array of exclusion objects to create exclusion specifiers for this artifact (ex: maven.exclusion("junit", "junit")).   |  `None` |
| <a id="maven.artifact-neverlink"></a>neverlink |  Determines if this artifact should be part of the runtime classpath.   |  `None` |
| <a id="maven.artifact-testonly"></a>testonly |  Determines whether this artifact is available for targets not marked as `testonly = True`.   |  `None` |
| <a id="maven.artifact-force_version"></a>force_version |  Whether the `version` is non-negotiable.   |  `False` |


<a id="maven.exclusion"></a>

## maven.exclusion

<pre>
load("@rules_jvm_external//:specs.bzl", "maven")

maven.exclusion(<a href="#maven.exclusion-group">group</a>, <a href="#maven.exclusion-artifact">artifact</a>)
</pre>

Generates the data map for a Maven artifact exclusion.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="maven.exclusion-group"></a>group |  The Maven group name of the dependency to exclude, e.g. "com.google.guava".   |  none |
| <a id="maven.exclusion-artifact"></a>artifact |  The Maven artifact name of the dependency to exclude, e.g. "guava".   |  none |


<a id="maven.repository"></a>

## maven.repository

<pre>
load("@rules_jvm_external//:specs.bzl", "maven")

maven.repository(<a href="#maven.repository-url">url</a>, <a href="#maven.repository-user">user</a>, <a href="#maven.repository-password">password</a>)
</pre>

Generates the data map for a Maven repository specifier given the available information.

If both a user and password are given as arguments, it will include the
access credentials in the repository spec. If one or both are missing, it
will just generate the repository url.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="maven.repository-url"></a>url |  A string containing the repository url (ex: "https://maven.google.com/").   |  none |
| <a id="maven.repository-user"></a>user |  A username for this Maven repository, if it requires authentication (ex: "johndoe").   |  `None` |
| <a id="maven.repository-password"></a>password |  A password for this Maven repository, if it requires authentication (ex: "example-password").   |  `None` |


