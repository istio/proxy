<!-- Generated with Stardoc, Do Not Edit! -->
# Aspects

The aspects described below are used within the build rule implementations.
Clients interested in writing custom rules that interface with the rules/provides
in this package might needs them to provide some of the same information.

On this page:

  * [swift_usage_aspect](#swift_usage_aspect)

<a id="swift_usage_aspect"></a>

## swift_usage_aspect

<pre>
swift_usage_aspect(<a href="#swift_usage_aspect-name">name</a>)
</pre>

Collects information about how Swift is used in a dependency tree.

When attached to an attribute, this aspect will propagate a `SwiftUsageInfo`
provider for any target found in that attribute that uses Swift, either directly
or deeper in its dependency tree. Conversely, if neither a target nor its
transitive dependencies use Swift, the `SwiftUsageInfo` provider will not be
propagated.

We use an aspect (as opposed to propagating this information through normal
providers returned by `swift_library`) because the information is needed if
Swift is used _anywhere_ in a dependency graph, even as dependencies of other
language rules that wouldn't know how to propagate the Swift-specific providers.

**ASPECT ATTRIBUTES**


| Name | Type |
| :------------- | :------------- |
| deps| String |


**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="swift_usage_aspect-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |


