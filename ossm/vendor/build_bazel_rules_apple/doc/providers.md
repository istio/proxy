<!-- Generated with Stardoc: http://skydoc.bazel.build -->

# Providers

Defines providers and related types used throughout the rules in this repository.

Most users will not need to use these providers to simply create and build Apple
targets, but if you want to write your own custom rules that interact with these
rules, then you will use these providers to communicate between them.

These providers are part of the public API of the bundling rules. Other rules that want to propagate
information to the bundling rules or that want to consume the bundling rules as their own inputs
should use these to handle the relevant information that they need.

Public initializers must be defined in apple:providers.bzl instead of apple/internal:providers.bzl.
These should build from the "raw initializer" where possible, but not export it, to allow for a safe
boundary with well-defined public APIs for broader usage.

<a id="AppleBaseBundleIdInfo"></a>

## AppleBaseBundleIdInfo

<pre>
AppleBaseBundleIdInfo(<a href="#AppleBaseBundleIdInfo-base_bundle_id">base_bundle_id</a>)
</pre>

Provides the base bundle ID prefix for an Apple rule.

**FIELDS**


| Name  | Description |
| :------------- | :------------- |
| <a id="AppleBaseBundleIdInfo-base_bundle_id"></a>base_bundle_id |  `String`. The bundle ID prefix, composed from an organization ID and an optional variant name.    |


<a id="AppleBinaryInfo"></a>

## AppleBinaryInfo

<pre>
AppleBinaryInfo(<a href="#AppleBinaryInfo-binary">binary</a>, <a href="#AppleBinaryInfo-infoplist">infoplist</a>, <a href="#AppleBinaryInfo-product_type">product_type</a>)
</pre>

Provides information about an Apple binary target.

This provider propagates general information about an Apple binary that is not
specific to any particular binary type.

**FIELDS**


| Name  | Description |
| :------------- | :------------- |
| <a id="AppleBinaryInfo-binary"></a>binary |  `File`. The binary (executable, dynamic library, etc.) file that the target represents.    |
| <a id="AppleBinaryInfo-infoplist"></a>infoplist |  `File`. The complete (binary-formatted) `Info.plist` embedded in the binary.    |
| <a id="AppleBinaryInfo-product_type"></a>product_type |  `String`. The dot-separated product type identifier associated with the binary (for example, `com.apple.product-type.tool`).    |


<a id="AppleBinaryInfoplistInfo"></a>

## AppleBinaryInfoplistInfo

<pre>
AppleBinaryInfoplistInfo(<a href="#AppleBinaryInfoplistInfo-infoplist">infoplist</a>)
</pre>

Provides information about the Info.plist that was linked into an Apple binary
target.

**FIELDS**


| Name  | Description |
| :------------- | :------------- |
| <a id="AppleBinaryInfoplistInfo-infoplist"></a>infoplist |  `File`. The complete (binary-formatted) `Info.plist` embedded in the binary.    |


<a id="AppleBundleInfo"></a>

## AppleBundleInfo

<pre>
AppleBundleInfo(<a href="#AppleBundleInfo-archive">archive</a>, <a href="#AppleBundleInfo-archive_root">archive_root</a>, <a href="#AppleBundleInfo-binary">binary</a>, <a href="#AppleBundleInfo-bundle_extension">bundle_extension</a>, <a href="#AppleBundleInfo-bundle_id">bundle_id</a>, <a href="#AppleBundleInfo-bundle_name">bundle_name</a>,
                <a href="#AppleBundleInfo-entitlements">entitlements</a>, <a href="#AppleBundleInfo-executable_name">executable_name</a>, <a href="#AppleBundleInfo-extension_safe">extension_safe</a>, <a href="#AppleBundleInfo-infoplist">infoplist</a>,
                <a href="#AppleBundleInfo-minimum_deployment_os_version">minimum_deployment_os_version</a>, <a href="#AppleBundleInfo-minimum_os_version">minimum_os_version</a>, <a href="#AppleBundleInfo-platform_type">platform_type</a>, <a href="#AppleBundleInfo-product_type">product_type</a>,
                <a href="#AppleBundleInfo-uses_swift">uses_swift</a>)
</pre>

This provider propagates general information about an Apple bundle that is not
specific to any particular bundle type. It is propagated by most bundling
rules (applications, extensions, frameworks, test bundles, and so forth).

**FIELDS**


| Name  | Description |
| :------------- | :------------- |
| <a id="AppleBundleInfo-archive"></a>archive |  `File`. The archive that contains the built bundle.    |
| <a id="AppleBundleInfo-archive_root"></a>archive_root |  `String`. The file system path (relative to the workspace root) where the signed bundle was constructed (before archiving). Other rules **should not** depend on this field; it is intended to support IDEs that want to read that path from the provider to avoid performance issues from unzipping the output archive.    |
| <a id="AppleBundleInfo-binary"></a>binary |  `File`. The binary (executable, dynamic library, etc.) that was bundled. The physical file is identical to the one inside the bundle except that it is always unsigned, so note that it is _not_ a path to the binary inside your output bundle. The primary purpose of this field is to provide a way to access the binary directly at analysis time; for example, for code coverage.    |
| <a id="AppleBundleInfo-bundle_extension"></a>bundle_extension |  `String`. The bundle extension.    |
| <a id="AppleBundleInfo-bundle_id"></a>bundle_id |  `String`. The bundle identifier (i.e., `CFBundleIdentifier` in `Info.plist`) of the bundle.    |
| <a id="AppleBundleInfo-bundle_name"></a>bundle_name |  `String`. The name of the bundle, without the extension.    |
| <a id="AppleBundleInfo-entitlements"></a>entitlements |  `File`. Entitlements file used, if any.    |
| <a id="AppleBundleInfo-executable_name"></a>executable_name |  `string`. The name of the executable that was bundled.    |
| <a id="AppleBundleInfo-extension_safe"></a>extension_safe |  `Boolean`. True if the target propagating this provider was compiled and linked with -application-extension, restricting it to extension-safe APIs only.    |
| <a id="AppleBundleInfo-infoplist"></a>infoplist |  `File`. The complete (binary-formatted) `Info.plist` file for the bundle.    |
| <a id="AppleBundleInfo-minimum_deployment_os_version"></a>minimum_deployment_os_version |  `string`. The minimum deployment OS version (as a dotted version number like "9.0") that this bundle was built to support. This is different from `minimum_os_version`, which is effective at compile time. Ensure version specific APIs are guarded with `available` clauses.    |
| <a id="AppleBundleInfo-minimum_os_version"></a>minimum_os_version |  `String`. The minimum OS version (as a dotted version number like "9.0") that this bundle was built to support.    |
| <a id="AppleBundleInfo-platform_type"></a>platform_type |  `String`. The platform type for the bundle (i.e. `ios` for iOS bundles).    |
| <a id="AppleBundleInfo-product_type"></a>product_type |  `String`. The dot-separated product type identifier associated with the bundle (for example, `com.apple.product-type.application`).    |
| <a id="AppleBundleInfo-uses_swift"></a>uses_swift |  Boolean. True if Swift is used by the target propagating this provider. This does not consider embedded bundles; for example, an Objective-C application containing a Swift extension would have this field set to true for the extension but false for the application.    |


<a id="AppleBundleVersionInfo"></a>

## AppleBundleVersionInfo

<pre>
AppleBundleVersionInfo(<a href="#AppleBundleVersionInfo-version_file">version_file</a>)
</pre>

Provides versioning information for an Apple bundle.

**FIELDS**


| Name  | Description |
| :------------- | :------------- |
| <a id="AppleBundleVersionInfo-version_file"></a>version_file |  Required. A `File` containing JSON-formatted text describing the version number information propagated by the target.<br><br>It contains two keys:<br><br>*   `build_version`, which corresponds to `CFBundleVersion`.<br><br>*   `short_version_string`, which corresponds to `CFBundleShortVersionString`.    |


<a id="AppleCodesigningDossierInfo"></a>

## AppleCodesigningDossierInfo

<pre>
AppleCodesigningDossierInfo(<a href="#AppleCodesigningDossierInfo-dossier">dossier</a>)
</pre>

Provides information around the use of a code signing dossier.

**FIELDS**


| Name  | Description |
| :------------- | :------------- |
| <a id="AppleCodesigningDossierInfo-dossier"></a>dossier |  A `File` reference to the code signing dossier zip that acts as a direct dependency of the given target if one was generated.    |


<a id="AppleDebugOutputsInfo"></a>

## AppleDebugOutputsInfo

<pre>
AppleDebugOutputsInfo(<a href="#AppleDebugOutputsInfo-outputs_map">outputs_map</a>)
</pre>

Holds debug outputs of an Apple binary rule.

This provider is DEPRECATED. Preferably use `AppleDsymBundleInfo` instead.

The only field is `output_map`, which is a dictionary of:
  `{ arch: { "dsym_binary": File, "linkmap": File }`

Where `arch` is any Apple architecture such as "arm64" or "armv7".

**FIELDS**


| Name  | Description |
| :------------- | :------------- |
| <a id="AppleDebugOutputsInfo-outputs_map"></a>outputs_map |  -    |


<a id="AppleDeviceTestRunnerInfo"></a>

## AppleDeviceTestRunnerInfo

<pre>
AppleDeviceTestRunnerInfo(<a href="#AppleDeviceTestRunnerInfo-device_type">device_type</a>, <a href="#AppleDeviceTestRunnerInfo-os_version">os_version</a>)
</pre>

Provider that device-based runner targets must propagate.

**FIELDS**


| Name  | Description |
| :------------- | :------------- |
| <a id="AppleDeviceTestRunnerInfo-device_type"></a>device_type |  The device type of the iOS simulator to run test. The supported types correspond to the output of `xcrun simctl list devicetypes`. E.g., iPhone X, iPad Air.    |
| <a id="AppleDeviceTestRunnerInfo-os_version"></a>os_version |  The os version of the iOS simulator to run test. The supported os versions correspond to the output of `xcrun simctl list runtimes`. E.g., 15.5.    |


<a id="AppleDsymBundleInfo"></a>

## AppleDsymBundleInfo

<pre>
AppleDsymBundleInfo(<a href="#AppleDsymBundleInfo-direct_dsyms">direct_dsyms</a>, <a href="#AppleDsymBundleInfo-transitive_dsyms">transitive_dsyms</a>)
</pre>

Provides information for an Apple dSYM bundle.

**FIELDS**


| Name  | Description |
| :------------- | :------------- |
| <a id="AppleDsymBundleInfo-direct_dsyms"></a>direct_dsyms |  `List` containing `File` references to each of the dSYM bundles that act as direct dependencies of the given target if any were generated.    |
| <a id="AppleDsymBundleInfo-transitive_dsyms"></a>transitive_dsyms |  `depset` containing `File` references to each of the dSYM bundles that act as transitive dependencies of the given target if any were generated.    |


<a id="AppleDynamicFrameworkInfo"></a>

## AppleDynamicFrameworkInfo

<pre>
AppleDynamicFrameworkInfo(<a href="#AppleDynamicFrameworkInfo-framework_dirs">framework_dirs</a>, <a href="#AppleDynamicFrameworkInfo-framework_files">framework_files</a>, <a href="#AppleDynamicFrameworkInfo-binary">binary</a>, <a href="#AppleDynamicFrameworkInfo-cc_info">cc_info</a>)
</pre>

Contains information about an Apple dynamic framework.

**FIELDS**


| Name  | Description |
| :------------- | :------------- |
| <a id="AppleDynamicFrameworkInfo-framework_dirs"></a>framework_dirs |  The framework path names used as link inputs in order to link against the dynamic framework.    |
| <a id="AppleDynamicFrameworkInfo-framework_files"></a>framework_files |  The full set of artifacts that should be included as inputs to link against the dynamic framework.    |
| <a id="AppleDynamicFrameworkInfo-binary"></a>binary |  The dylib binary artifact of the dynamic framework.    |
| <a id="AppleDynamicFrameworkInfo-cc_info"></a>cc_info |  A `CcInfo` which contains information about the transitive dependencies linked into the binary.    |


<a id="AppleExecutableBinaryInfo"></a>

## AppleExecutableBinaryInfo

<pre>
AppleExecutableBinaryInfo(<a href="#AppleExecutableBinaryInfo-binary">binary</a>, <a href="#AppleExecutableBinaryInfo-cc_info">cc_info</a>)
</pre>

Contains the executable binary output that was built using
`link_multi_arch_binary` with the `executable` binary type.

**FIELDS**


| Name  | Description |
| :------------- | :------------- |
| <a id="AppleExecutableBinaryInfo-binary"></a>binary |  The executable binary artifact output by `link_multi_arch_binary`.    |
| <a id="AppleExecutableBinaryInfo-cc_info"></a>cc_info |  A `CcInfo` which contains information about the transitive dependencies linked into the binary.    |


<a id="AppleExtraOutputsInfo"></a>

## AppleExtraOutputsInfo

<pre>
AppleExtraOutputsInfo(<a href="#AppleExtraOutputsInfo-files">files</a>)
</pre>

Provides information about extra outputs that should be produced from the build.

This provider propagates supplemental files that should be produced as outputs
even if the bundle they are associated with is not a direct output of the rule.
For example, an application that contains an extension will build both targets
but only the application will be a rule output. However, if dSYM bundles are
also being generated, we do want to produce the dSYMs for *both* application and
extension as outputs of the build, not just the dSYMs of the explicit target
being built (the application).

**FIELDS**


| Name  | Description |
| :------------- | :------------- |
| <a id="AppleExtraOutputsInfo-files"></a>files |  `depset` of `File`s. These files will be propagated from embedded bundles (such as frameworks and extensions) to the top-level bundle (such as an application) to ensure that they are explicitly produced as outputs of the build.    |


<a id="AppleFrameworkBundleInfo"></a>

## AppleFrameworkBundleInfo

<pre>
AppleFrameworkBundleInfo()
</pre>

Denotes a target is an Apple framework bundle.

This provider does not reference 3rd party or precompiled frameworks.
Propagated by Apple framework rules: `ios_framework`, and `tvos_framework`.

**FIELDS**



<a id="AppleFrameworkImportInfo"></a>

## AppleFrameworkImportInfo

<pre>
AppleFrameworkImportInfo(<a href="#AppleFrameworkImportInfo-framework_imports">framework_imports</a>, <a href="#AppleFrameworkImportInfo-dsym_imports">dsym_imports</a>, <a href="#AppleFrameworkImportInfo-build_archs">build_archs</a>, <a href="#AppleFrameworkImportInfo-debug_info_binaries">debug_info_binaries</a>)
</pre>

Provider that propagates information about 3rd party imported framework targets.

Propagated by framework and XCFramework import rules: `apple_dynamic_framework_import`,
`apple_dynamic_xcframework_import`, `apple_static_framework_import`, and
`apple_static_xcframework_import`

**FIELDS**


| Name  | Description |
| :------------- | :------------- |
| <a id="AppleFrameworkImportInfo-framework_imports"></a>framework_imports |  `depset` of `File`s that represent framework imports that need to be bundled in the top level application bundle under the Frameworks directory.    |
| <a id="AppleFrameworkImportInfo-dsym_imports"></a>dsym_imports |  Depset of Files that represent dSYM imports that need to be processed to provide .symbols files for packaging into the .ipa file if requested in the build with --define=apple.package_symbols=(yes\|true\|1).    |
| <a id="AppleFrameworkImportInfo-build_archs"></a>build_archs |  `depset` of `String`s that represent binary architectures reported from the current build.    |
| <a id="AppleFrameworkImportInfo-debug_info_binaries"></a>debug_info_binaries |  Depset of Files that represent framework binaries and dSYM binaries that provide debug info.    |


<a id="ApplePlatformInfo"></a>

## ApplePlatformInfo

<pre>
ApplePlatformInfo(<a href="#ApplePlatformInfo-target_os">target_os</a>, <a href="#ApplePlatformInfo-target_arch">target_arch</a>, <a href="#ApplePlatformInfo-target_environment">target_environment</a>)
</pre>

Provides information for the currently selected Apple platforms.

**FIELDS**


| Name  | Description |
| :------------- | :------------- |
| <a id="ApplePlatformInfo-target_os"></a>target_os |  `String` representing the selected Apple OS.    |
| <a id="ApplePlatformInfo-target_arch"></a>target_arch |  `String` representing the selected target architecture or cpu type.    |
| <a id="ApplePlatformInfo-target_environment"></a>target_environment |  `String` representing the selected target environment (e.g. "device", "simulator").    |


<a id="AppleProvisioningProfileInfo"></a>

## AppleProvisioningProfileInfo

<pre>
AppleProvisioningProfileInfo(<a href="#AppleProvisioningProfileInfo-provisioning_profile">provisioning_profile</a>, <a href="#AppleProvisioningProfileInfo-profile_name">profile_name</a>, <a href="#AppleProvisioningProfileInfo-team_id">team_id</a>)
</pre>

Provides information about a provisioning profile.

**FIELDS**


| Name  | Description |
| :------------- | :------------- |
| <a id="AppleProvisioningProfileInfo-provisioning_profile"></a>provisioning_profile |  `File`. The provisioning profile.    |
| <a id="AppleProvisioningProfileInfo-profile_name"></a>profile_name |  string. The profile name (e.g. "iOS Team Provisioning Profile: com.example.app").    |
| <a id="AppleProvisioningProfileInfo-team_id"></a>team_id |  `string`. The Team ID the profile is associated with (e.g. "A12B3CDEFG"), or `None` if it's not known at analysis time.    |


<a id="AppleResourceBundleInfo"></a>

## AppleResourceBundleInfo

<pre>
AppleResourceBundleInfo()
</pre>

Denotes that a target is an Apple resource bundle.

This provider does not contain any fields of its own at this time but is used as
a "marker" to indicate that a target is specifically an Apple resource bundle
(and not some other Apple bundle). Rule authors who wish to require that a
dependency is an Apple resource bundle should use this provider to describe that
requirement.

**FIELDS**



<a id="AppleResourceInfo"></a>

## AppleResourceInfo

<pre>
AppleResourceInfo(<a href="#AppleResourceInfo-alternate_icons">alternate_icons</a>, <a href="#AppleResourceInfo-asset_catalogs">asset_catalogs</a>, <a href="#AppleResourceInfo-datamodels">datamodels</a>, <a href="#AppleResourceInfo-framework">framework</a>, <a href="#AppleResourceInfo-infoplists">infoplists</a>, <a href="#AppleResourceInfo-metals">metals</a>,
                  <a href="#AppleResourceInfo-mlmodels">mlmodels</a>, <a href="#AppleResourceInfo-plists">plists</a>, <a href="#AppleResourceInfo-pngs">pngs</a>, <a href="#AppleResourceInfo-processed">processed</a>, <a href="#AppleResourceInfo-storyboards">storyboards</a>, <a href="#AppleResourceInfo-strings">strings</a>, <a href="#AppleResourceInfo-texture_atlases">texture_atlases</a>,
                  <a href="#AppleResourceInfo-unprocessed">unprocessed</a>, <a href="#AppleResourceInfo-xibs">xibs</a>, <a href="#AppleResourceInfo-owners">owners</a>, <a href="#AppleResourceInfo-processed_origins">processed_origins</a>, <a href="#AppleResourceInfo-unowned_resources">unowned_resources</a>)
</pre>

Provider that propagates buckets of resources that are differentiated by type.

**FIELDS**


| Name  | Description |
| :------------- | :------------- |
| <a id="AppleResourceInfo-alternate_icons"></a>alternate_icons |  Alternate icons to be included in the App bundle.    |
| <a id="AppleResourceInfo-asset_catalogs"></a>asset_catalogs |  Resources that need to be embedded into Assets.car.    |
| <a id="AppleResourceInfo-datamodels"></a>datamodels |  Datamodel files.    |
| <a id="AppleResourceInfo-framework"></a>framework |  Apple framework bundle from `ios_framework` and `tvos_framework` targets.    |
| <a id="AppleResourceInfo-infoplists"></a>infoplists |  Plist files to be merged and processed. Plist files that should not be merged into the root Info.plist should be propagated in `plists`. Because of this, infoplists should only be bucketed with the `bucketize_typed` method.    |
| <a id="AppleResourceInfo-metals"></a>metals |  Metal Shading Language source files to be compiled into a single .metallib file and bundled at the top level.    |
| <a id="AppleResourceInfo-mlmodels"></a>mlmodels |  Core ML model files that should be processed and bundled at the top level.    |
| <a id="AppleResourceInfo-plists"></a>plists |  Resource Plist files that should not be merged into Info.plist    |
| <a id="AppleResourceInfo-pngs"></a>pngs |  PNG images which are not bundled in an .xcassets folder.    |
| <a id="AppleResourceInfo-processed"></a>processed |  Typed resources that have already been processed.    |
| <a id="AppleResourceInfo-storyboards"></a>storyboards |  Storyboard files.    |
| <a id="AppleResourceInfo-strings"></a>strings |  Localization strings files.    |
| <a id="AppleResourceInfo-texture_atlases"></a>texture_atlases |  Texture atlas files.    |
| <a id="AppleResourceInfo-unprocessed"></a>unprocessed |  Generic resources not mapped to the other types.    |
| <a id="AppleResourceInfo-xibs"></a>xibs |  XIB Interface files.    |
| <a id="AppleResourceInfo-owners"></a>owners |  `depset` of (resource, owner) pairs.    |
| <a id="AppleResourceInfo-processed_origins"></a>processed_origins |  `depset` of (processed resource, resource list) pairs.    |
| <a id="AppleResourceInfo-unowned_resources"></a>unowned_resources |  `depset` of unowned resources.    |


<a id="AppleSharedCapabilityInfo"></a>

## AppleSharedCapabilityInfo

<pre>
AppleSharedCapabilityInfo(<a href="#AppleSharedCapabilityInfo-base_bundle_id">base_bundle_id</a>)
</pre>

Provides information on a mergeable set of shared capabilities.

**FIELDS**


| Name  | Description |
| :------------- | :------------- |
| <a id="AppleSharedCapabilityInfo-base_bundle_id"></a>base_bundle_id |  `String`. The bundle ID prefix, composed from an organization ID and an optional variant name.    |


<a id="AppleStaticXcframeworkBundleInfo"></a>

## AppleStaticXcframeworkBundleInfo

<pre>
AppleStaticXcframeworkBundleInfo()
</pre>

Denotes that a target is a static library XCFramework.

This provider does not contain any fields of its own at this time but is used as
a "marker" to indicate that a target is specifically an XCFramework bundle
(and not some other Apple bundle). Rule authors who wish to require that a
dependency is an XCFramework should use this provider to describe that
requirement.

**FIELDS**



<a id="AppleTestInfo"></a>

## AppleTestInfo

<pre>
AppleTestInfo(<a href="#AppleTestInfo-includes">includes</a>, <a href="#AppleTestInfo-module_maps">module_maps</a>, <a href="#AppleTestInfo-module_name">module_name</a>, <a href="#AppleTestInfo-non_arc_sources">non_arc_sources</a>, <a href="#AppleTestInfo-sources">sources</a>, <a href="#AppleTestInfo-swift_modules">swift_modules</a>,
              <a href="#AppleTestInfo-test_bundle">test_bundle</a>, <a href="#AppleTestInfo-test_host">test_host</a>, <a href="#AppleTestInfo-deps">deps</a>)
</pre>

Provider that test targets propagate to be used for IDE integration.

This includes information regarding test source files, transitive include paths,
transitive module maps, and transitive Swift modules. Test source files are
considered to be all of which belong to the first-level dependencies on the test
target.

**FIELDS**


| Name  | Description |
| :------------- | :------------- |
| <a id="AppleTestInfo-includes"></a>includes |  `depset` of `String`s representing transitive include paths which are needed by IDEs to be used for indexing the test sources.    |
| <a id="AppleTestInfo-module_maps"></a>module_maps |  `depset` of `File`s representing module maps which are needed by IDEs to be used for indexing the test sources.    |
| <a id="AppleTestInfo-module_name"></a>module_name |  `String` representing the module name used by the test's sources. This is only set if the test only contains a single top-level Swift dependency. This may be used by an IDE to identify the Swift module (if any) used by the test's sources.    |
| <a id="AppleTestInfo-non_arc_sources"></a>non_arc_sources |  `depset` of `File`s containing non-ARC sources from the test's immediate deps.    |
| <a id="AppleTestInfo-sources"></a>sources |  `depset` of `File`s containing sources and headers from the test's immediate deps.    |
| <a id="AppleTestInfo-swift_modules"></a>swift_modules |  `depset` of `File`s representing transitive swift modules which are needed by IDEs to be used for indexing the test sources.    |
| <a id="AppleTestInfo-test_bundle"></a>test_bundle |  The artifact representing the XCTest bundle for the test target.    |
| <a id="AppleTestInfo-test_host"></a>test_host |  The artifact representing the test host for the test target, if the test requires a test host.    |
| <a id="AppleTestInfo-deps"></a>deps |  `depset` of `String`s representing the labels of all immediate deps of the test. Only source files from these deps will be present in `sources`. This may be used by IDEs to differentiate a test target's transitive module maps from its direct module maps, as including the direct module maps may break indexing for the source files of the immediate deps.    |


<a id="AppleTestRunnerInfo"></a>

## AppleTestRunnerInfo

<pre>
AppleTestRunnerInfo(<a href="#AppleTestRunnerInfo-execution_requirements">execution_requirements</a>, <a href="#AppleTestRunnerInfo-execution_environment">execution_environment</a>, <a href="#AppleTestRunnerInfo-test_environment">test_environment</a>,
                    <a href="#AppleTestRunnerInfo-test_runner_template">test_runner_template</a>)
</pre>

Provider that runner targets must propagate.

In addition to the fields, all the runfiles that the runner target declares will be added to the
test rules runfiles.

**FIELDS**


| Name  | Description |
| :------------- | :------------- |
| <a id="AppleTestRunnerInfo-execution_requirements"></a>execution_requirements |  Optional dictionary that represents the specific hardware requirements for this test.    |
| <a id="AppleTestRunnerInfo-execution_environment"></a>execution_environment |  Optional dictionary with the environment variables that are to be set in the test action, and are not propagated into the XCTest invocation. These values will _not_ be added into the %(test_env)s substitution, but will be set in the test action.    |
| <a id="AppleTestRunnerInfo-test_environment"></a>test_environment |  Optional dictionary with the environment variables that are to be propagated into the XCTest invocation. These values will be included in the %(test_env)s substitution and will _not_ be set in the test action.    |
| <a id="AppleTestRunnerInfo-test_runner_template"></a>test_runner_template |  Required template file that contains the specific mechanism with which the tests will be run. The *_ui_test and *_unit_test rules will substitute the following values:     * %(test_host_path)s:   Path to the app being tested.     * %(test_bundle_path)s: Path to the test bundle that contains the tests.     * %(test_env)s:         Environment variables for the XCTest invocation (e.g FOO=BAR,BAZ=QUX).     * %(test_type)s:        The test type, whether it is unit or UI.    |


<a id="AppleXcframeworkBundleInfo"></a>

## AppleXcframeworkBundleInfo

<pre>
AppleXcframeworkBundleInfo()
</pre>

Denotes that a target is an XCFramework.

This provider does not contain any fields of its own at this time but is used as
a "marker" to indicate that a target is specifically an XCFramework bundle
(and not some other Apple bundle). Rule authors who wish to require that a
dependency is an XCFramework should use this provider to describe that
requirement.

**FIELDS**



<a id="DocCBundleInfo"></a>

## DocCBundleInfo

<pre>
DocCBundleInfo(<a href="#DocCBundleInfo-bundle">bundle</a>, <a href="#DocCBundleInfo-bundle_files">bundle_files</a>)
</pre>

Provides general information about a .docc bundle.

**FIELDS**


| Name  | Description |
| :------------- | :------------- |
| <a id="DocCBundleInfo-bundle"></a>bundle |  the path to the .docc bundle    |
| <a id="DocCBundleInfo-bundle_files"></a>bundle_files |  the file targets contained within the .docc bundle    |


<a id="DocCSymbolGraphsInfo"></a>

## DocCSymbolGraphsInfo

<pre>
DocCSymbolGraphsInfo(<a href="#DocCSymbolGraphsInfo-symbol_graphs">symbol_graphs</a>)
</pre>

Provides the symbol graphs required to archive a .docc bundle.

**FIELDS**


| Name  | Description |
| :------------- | :------------- |
| <a id="DocCSymbolGraphsInfo-symbol_graphs"></a>symbol_graphs |  the depset of paths to the symbol graphs    |


<a id="IosAppClipBundleInfo"></a>

## IosAppClipBundleInfo

<pre>
IosAppClipBundleInfo()
</pre>

Denotes that a target is an iOS app clip.

This provider does not contain any fields of its own at this time but is used as
a "marker" to indicate that a target is specifically an iOS app clip bundle (and
not some other Apple bundle). Rule authors who wish to require that a dependency
is an iOS app clip should use this provider to describe that requirement.

**FIELDS**



<a id="IosApplicationBundleInfo"></a>

## IosApplicationBundleInfo

<pre>
IosApplicationBundleInfo()
</pre>

Denotes that a target is an iOS application.

This provider does not contain any fields of its own at this time but is used as
a "marker" to indicate that a target is specifically an iOS application bundle
(and not some other Apple bundle). Rule authors who wish to require that a
dependency is an iOS application should use this provider to describe that
requirement.

**FIELDS**



<a id="IosExtensionBundleInfo"></a>

## IosExtensionBundleInfo

<pre>
IosExtensionBundleInfo()
</pre>

Denotes that a target is an iOS application extension.

This provider does not contain any fields of its own at this time but is used as
a "marker" to indicate that a target is specifically an iOS application
extension bundle (and not some other Apple bundle). Rule authors who wish to
require that a dependency is an iOS application extension should use this
provider to describe that requirement.

**FIELDS**



<a id="IosFrameworkBundleInfo"></a>

## IosFrameworkBundleInfo

<pre>
IosFrameworkBundleInfo()
</pre>

Denotes that a target is an iOS dynamic framework.

This provider does not contain any fields of its own at this time but is used as
a "marker" to indicate that a target is specifically an iOS dynamic framework
bundle (and not some other Apple bundle). Rule authors who wish to require that
a dependency is an iOS dynamic framework should use this provider to describe
that requirement.

**FIELDS**



<a id="IosImessageApplicationBundleInfo"></a>

## IosImessageApplicationBundleInfo

<pre>
IosImessageApplicationBundleInfo()
</pre>

Denotes that a target is an iOS iMessage application.

This provider does not contain any fields of its own at this time but is used as
a "marker" to indicate that a target is specifically an iOS iMessage application
bundle (and not some other Apple bundle). Rule authors who wish to require that
a dependency is an iOS iMessage application should use this provider to describe
that requirement.

**FIELDS**



<a id="IosImessageExtensionBundleInfo"></a>

## IosImessageExtensionBundleInfo

<pre>
IosImessageExtensionBundleInfo()
</pre>

Denotes that a target is an iOS iMessage extension.

This provider does not contain any fields of its own at this time but is used as
a "marker" to indicate that a target is specifically an iOS iMessage extension
bundle (and not some other Apple bundle). Rule authors who wish to require that
a dependency is an iOS iMessage extension should use this provider to describe
that requirement.

**FIELDS**



<a id="IosStaticFrameworkBundleInfo"></a>

## IosStaticFrameworkBundleInfo

<pre>
IosStaticFrameworkBundleInfo()
</pre>

Denotes that a target is an iOS static framework.

This provider does not contain any fields of its own at this time but is used as
a "marker" to indicate that a target is specifically an iOS static framework
bundle (and not some other Apple bundle). Rule authors who wish to require that
a dependency is an iOS static framework should use this provider to describe
that requirement.

**FIELDS**



<a id="IosStickerPackExtensionBundleInfo"></a>

## IosStickerPackExtensionBundleInfo

<pre>
IosStickerPackExtensionBundleInfo()
</pre>

Denotes that a target is an iOS Sticker Pack extension.

This provider does not contain any fields of its own at this time but is used as
a "marker" to indicate that a target is specifically an iOS Sticker Pack extension
bundle (and not some other Apple bundle). Rule authors who wish to require that
a dependency is an iOS Sticker Pack extension should use this provider to describe
that requirement.

**FIELDS**



<a id="IosXcTestBundleInfo"></a>

## IosXcTestBundleInfo

<pre>
IosXcTestBundleInfo()
</pre>

Denotes a target that is an iOS .xctest bundle.

This provider does not contain any fields of its own at this time but is used as
a "marker" to indicate that a target is specifically an iOS .xctest bundle (and
not some other Apple bundle). Rule authors who wish to require that a dependency
is an iOS .xctest bundle should use this provider to describe that requirement.

**FIELDS**



<a id="MacosApplicationBundleInfo"></a>

## MacosApplicationBundleInfo

<pre>
MacosApplicationBundleInfo()
</pre>

Denotes that a target is a macOS application.

This provider does not contain any fields of its own at this time but is used as
a "marker" to indicate that a target is specifically a macOS application bundle
(and not some other Apple bundle). Rule authors who wish to require that a
dependency is a macOS application should use this provider to describe that
requirement.

**FIELDS**



<a id="MacosBundleBundleInfo"></a>

## MacosBundleBundleInfo

<pre>
MacosBundleBundleInfo()
</pre>

Denotes that a target is a macOS loadable bundle.

This provider does not contain any fields of its own at this time but is used as
a "marker" to indicate that a target is specifically a macOS loadable bundle
(and not some other Apple bundle). Rule authors who wish to require that a
dependency is a macOS loadable bundle should use this provider to describe that
requirement.

**FIELDS**



<a id="MacosExtensionBundleInfo"></a>

## MacosExtensionBundleInfo

<pre>
MacosExtensionBundleInfo()
</pre>

Denotes that a target is a macOS application extension.

This provider does not contain any fields of its own at this time but is used as
a "marker" to indicate that a target is specifically a macOS application
extension bundle (and not some other Apple bundle). Rule authors who wish to
require that a dependency is a macOS application extension should use this
provider to describe that requirement.

**FIELDS**



<a id="MacosFrameworkBundleInfo"></a>

## MacosFrameworkBundleInfo

<pre>
MacosFrameworkBundleInfo()
</pre>

Denotes that a target is an macOS dynamic framework.

This provider does not contain any fields of its own at this time but is used as
a "marker" to indicate that a target is specifically an macOS dynamic framework
bundle (and not some other Apple bundle). Rule authors who wish to require that
a dependency is an macOS dynamic framework should use this provider to describe
that requirement.

**FIELDS**



<a id="MacosKernelExtensionBundleInfo"></a>

## MacosKernelExtensionBundleInfo

<pre>
MacosKernelExtensionBundleInfo()
</pre>

Denotes that a target is a macOS kernel extension.

This provider does not contain any fields of its own at this time but is used as
a "marker" to indicate that a target is specifically a macOS kernel extension
(and not some other Apple bundle). Rule authors who wish to require that a
dependency is a macOS kernel extension should use this provider to describe that
requirement.

**FIELDS**



<a id="MacosQuickLookPluginBundleInfo"></a>

## MacosQuickLookPluginBundleInfo

<pre>
MacosQuickLookPluginBundleInfo()
</pre>

Denotes that a target is a macOS Quick Look Generator bundle.

This provider does not contain any fields of its own at this time but is used as
a "marker" to indicate that a target is specifically a macOS Quick Look generator
bundle (and not some other Apple bundle). Rule authors who wish to require that
a dependency is a macOS Quick Look generator should use this provider to describe
that requirement.

**FIELDS**



<a id="MacosSpotlightImporterBundleInfo"></a>

## MacosSpotlightImporterBundleInfo

<pre>
MacosSpotlightImporterBundleInfo()
</pre>

Denotes that a target is a macOS Spotlight Importer bundle.

This provider does not contain any fields of its own at this time but is used as
a "marker" to indicate that a target is specifically a macOS Spotlight importer
(and not some other Apple bundle). Rule authors who wish to require that a
dependency is a macOS Spotlight importer should use this provider to describe that
requirement.

**FIELDS**



<a id="MacosStaticFrameworkBundleInfo"></a>

## MacosStaticFrameworkBundleInfo

<pre>
MacosStaticFrameworkBundleInfo()
</pre>

Denotes that a target is an macOS static framework.

This provider does not contain any fields of its own at this time but is used as
a "marker" to indicate that a target is specifically an macOS static framework
bundle (and not some other Apple bundle). Rule authors who wish to require that
a dependency is an macOS static framework should use this provider to describe
that requirement.

**FIELDS**



<a id="MacosXPCServiceBundleInfo"></a>

## MacosXPCServiceBundleInfo

<pre>
MacosXPCServiceBundleInfo()
</pre>

Denotes that a target is a macOS XPC Service bundle.

This provider does not contain any fields of its own at this time but is used as
a "marker" to indicate that a target is specifically a macOS XPC service
(and not some other Apple bundle). Rule authors who wish to require that a
dependency is a macOS XPC service should use this provider to describe that
requirement.

**FIELDS**



<a id="MacosXcTestBundleInfo"></a>

## MacosXcTestBundleInfo

<pre>
MacosXcTestBundleInfo()
</pre>

Denotes a target that is a macOS .xctest bundle.

This provider does not contain any fields of its own at this time but is used as
a "marker" to indicate that a target is specifically a macOS .xctest bundle
(and not some other Apple bundle). Rule authors who wish to require that a
dependency is a macOS .xctest bundle should use this provider to describe that
requirement.

**FIELDS**



<a id="TvosApplicationBundleInfo"></a>

## TvosApplicationBundleInfo

<pre>
TvosApplicationBundleInfo()
</pre>

Denotes that a target is a tvOS application.

This provider does not contain any fields of its own at this time but is used as
a "marker" to indicate that a target is specifically a tvOS application bundle
(and not some other Apple bundle). Rule authors who wish to require that a
dependency is a tvOS application should use this provider to describe that
requirement.

**FIELDS**



<a id="TvosExtensionBundleInfo"></a>

## TvosExtensionBundleInfo

<pre>
TvosExtensionBundleInfo()
</pre>

Denotes that a target is a tvOS application extension.

This provider does not contain any fields of its own at this time but is used as
a "marker" to indicate that a target is specifically a tvOS application
extension bundle (and not some other Apple bundle). Rule authors who wish to
require that a dependency is a tvOS application extension should use this
provider to describe that requirement.

**FIELDS**



<a id="TvosFrameworkBundleInfo"></a>

## TvosFrameworkBundleInfo

<pre>
TvosFrameworkBundleInfo()
</pre>

Denotes that a target is a tvOS dynamic framework.

This provider does not contain any fields of its own at this time but is used as
a "marker" to indicate that a target is specifically a tvOS dynamic framework
bundle (and not some other Apple bundle). Rule authors who wish to require that
a dependency is a tvOS dynamic framework should use this provider to describe
that requirement.

**FIELDS**



<a id="TvosStaticFrameworkBundleInfo"></a>

## TvosStaticFrameworkBundleInfo

<pre>
TvosStaticFrameworkBundleInfo()
</pre>

Denotes that a target is a tvOS static framework.

This provider does not contain any fields of its own at this time but is used as
a "marker" to indicate that a target is specifically a tvOS static framework
bundle (and not some other Apple bundle). Rule authors who wish to require that
a dependency is a tvOS static framework should use this provider to describe
that requirement.

**FIELDS**



<a id="TvosXcTestBundleInfo"></a>

## TvosXcTestBundleInfo

<pre>
TvosXcTestBundleInfo()
</pre>

Denotes a target that is a tvOS .xctest bundle.

This provider does not contain any fields of its own at this time but is used as
a "marker" to indicate that a target is specifically a tvOS .xctest bundle (and
not some other Apple bundle). Rule authors who wish to require that a dependency
is a tvOS .xctest bundle should use this provider to describe that requirement.

**FIELDS**



<a id="VisionosApplicationBundleInfo"></a>

## VisionosApplicationBundleInfo

<pre>
VisionosApplicationBundleInfo()
</pre>

Denotes that a target is a visionOS application.

This provider does not contain any fields of its own at this time but is used as
a "marker" to indicate that a target is specifically a visionOS application
bundle (and not some other Apple bundle). Rule authors who wish to require that a
dependency is a visionOS application should use this provider to describe that
requirement.

**FIELDS**



<a id="VisionosExtensionBundleInfo"></a>

## VisionosExtensionBundleInfo

<pre>
VisionosExtensionBundleInfo()
</pre>

Denotes that a target is a visionOS application.

This provider does not contain any fields of its own at this time but is used as
a "marker" to indicate that a target is specifically a visionOS application
bundle (and not some other Apple bundle). Rule authors who wish to require that a
dependency is a visionOS application should use this provider to describe that
requirement.

**FIELDS**



<a id="VisionosFrameworkBundleInfo"></a>

## VisionosFrameworkBundleInfo

<pre>
VisionosFrameworkBundleInfo()
</pre>

Denotes that a target is visionOS dynamic framework.

This provider does not contain any fields of its own at this time but is used as
a "marker" to indicate that a target is specifically a visionOS dynamic framework
bundle (and not some other Apple bundle). Rule authors who wish to require that
a dependency is a visionOS dynamic framework should use this provider to describe
that requirement.

**FIELDS**



<a id="VisionosXcTestBundleInfo"></a>

## VisionosXcTestBundleInfo

<pre>
VisionosXcTestBundleInfo()
</pre>

Denotes a target that is a visionOS .xctest bundle.

This provider does not contain any fields of its own at this time but is used as
a "marker" to indicate that a target is specifically a visionOS .xctest bundle
(and not some other Apple bundle). Rule authors who wish to require that a
dependency is a visionOS .xctest bundle  should use this provider to describe
that requirement.

**FIELDS**



<a id="WatchosApplicationBundleInfo"></a>

## WatchosApplicationBundleInfo

<pre>
WatchosApplicationBundleInfo()
</pre>

Denotes that a target is a watchOS application.

This provider does not contain any fields of its own at this time but is used as
a "marker" to indicate that a target is specifically a watchOS application
bundle (and not some other Apple bundle). Rule authors who wish to require that
a dependency is a watchOS application should use this provider to describe that
requirement.

**FIELDS**



<a id="WatchosExtensionBundleInfo"></a>

## WatchosExtensionBundleInfo

<pre>
WatchosExtensionBundleInfo()
</pre>

Denotes that a target is a watchOS application extension.

This provider does not contain any fields of its own at this time but is used as
a "marker" to indicate that a target is specifically a watchOS application
extension bundle (and not some other Apple bundle). Rule authors who wish to
require that a dependency is a watchOS application extension should use this
provider to describe that requirement.

**FIELDS**



<a id="WatchosFrameworkBundleInfo"></a>

## WatchosFrameworkBundleInfo

<pre>
WatchosFrameworkBundleInfo()
</pre>

Denotes that a target is watchOS dynamic framework.

This provider does not contain any fields of its own at this time but is used as
a "marker" to indicate that a target is specifically a watchOS dynamic framework
bundle (and not some other Apple bundle). Rule authors who wish to require that
a dependency is a watchOS dynamic framework should use this provider to describe
that requirement.

**FIELDS**



<a id="WatchosStaticFrameworkBundleInfo"></a>

## WatchosStaticFrameworkBundleInfo

<pre>
WatchosStaticFrameworkBundleInfo()
</pre>

Denotes that a target is an watchOS static framework.

This provider does not contain any fields of its own at this time but is used as
a "marker" to indicate that a target is specifically a watchOS static framework
bundle (and not some other Apple bundle). Rule authors who wish to require that
a dependency is a watchOS static framework should use this provider to describe
that requirement.

**FIELDS**



<a id="WatchosXcTestBundleInfo"></a>

## WatchosXcTestBundleInfo

<pre>
WatchosXcTestBundleInfo()
</pre>

Denotes a target that is a watchOS .xctest bundle.

This provider does not contain any fields of its own at this time but is used as
a "marker" to indicate that a target is specifically a watchOS .xctest bundle (and
not some other Apple bundle). Rule authors who wish to require that a dependency
is a watchOS .xctest bundle should use this provider to describe that requirement.

**FIELDS**



<a id="apple_provider.make_apple_bundle_version_info"></a>

## apple_provider.make_apple_bundle_version_info

<pre>
apple_provider.make_apple_bundle_version_info(<a href="#apple_provider.make_apple_bundle_version_info-version_file">version_file</a>)
</pre>

Creates a new instance of the `AppleBundleVersionInfo` provider.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="apple_provider.make_apple_bundle_version_info-version_file"></a>version_file |  Required. See the docs on `AppleBundleVersionInfo`.   |  none |

**RETURNS**

A new `AppleBundleVersionInfo` provider based on the supplied arguments.


<a id="apple_provider.make_apple_test_runner_info"></a>

## apple_provider.make_apple_test_runner_info

<pre>
apple_provider.make_apple_test_runner_info(<a href="#apple_provider.make_apple_test_runner_info-kwargs">kwargs</a>)
</pre>

Creates a new instance of the AppleTestRunnerInfo provider.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="apple_provider.make_apple_test_runner_info-kwargs"></a>kwargs |  A set of keyword arguments expected to match the fields of `AppleTestRunnerInfo`. See the documentation for `AppleTestRunnerInfo` for what these must be.   |  none |

**RETURNS**

A new `AppleTestRunnerInfo` provider based on the supplied arguments.


<a id="apple_provider.merge_apple_framework_import_info"></a>

## apple_provider.merge_apple_framework_import_info

<pre>
apple_provider.merge_apple_framework_import_info(<a href="#apple_provider.merge_apple_framework_import_info-apple_framework_import_infos">apple_framework_import_infos</a>)
</pre>

Merges multiple `AppleFrameworkImportInfo` into one.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="apple_provider.merge_apple_framework_import_info-apple_framework_import_infos"></a>apple_framework_import_infos |  List of `AppleFrameworkImportInfo` to be merged.   |  none |

**RETURNS**

Result of merging all the received framework infos.


