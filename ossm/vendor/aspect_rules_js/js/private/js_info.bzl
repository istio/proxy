"JsInfo provider"

JsInfo = provider(
    doc = "Encapsulates information provided by rules in rules_js and derivative rule sets",
    fields = {
        "declarations": "A depset of declaration files produced by the target",
        "npm_linked_package_files": "A depset of node_modules links and bin files produced by the target",
        "npm_linked_packages": "A depset of NpmLinkedPackageInfo providers that are dependencies of the target",
        "npm_package_store_deps": "A depset of NpmPackageStoreInfo providers from non-dev npm dependencies of the target and the target's transitive dependencies to use as direct dependencies when linking downstream npm_package targets with npm_link_package",
        "sources": "A depset of source files produced by the target",
        "transitive_declarations": "A depset of declaration files produced by the target and the target's transitive deps",
        "transitive_npm_linked_package_files": "A depset of files in npm linked package dependencies of the target and the target's transitive deps",
        "transitive_npm_linked_packages": "A depset of NpmLinkedPackageInfo providers that are dependencies of the target and the target's transitive deps",
        "transitive_sources": "A depset of source files produced by the target and the target's transitive deps",
    },
)

def js_info(
        declarations = depset(),
        npm_linked_package_files = depset(),
        npm_linked_packages = depset(),
        npm_package_store_deps = depset(),
        sources = depset(),
        transitive_declarations = depset(),
        transitive_npm_linked_package_files = depset(),
        transitive_npm_linked_packages = depset(),
        transitive_sources = depset()):
    """Construct a JsInfo.

    Args:
        declarations: See JsInfo documentation
        npm_linked_package_files: See JsInfo documentation
        npm_linked_packages: See JsInfo documentation
        npm_package_store_deps: See JsInfo documentation
        sources: See JsInfo documentation
        transitive_declarations: See JsInfo documentation
        transitive_npm_linked_package_files: See JsInfo documentation
        transitive_npm_linked_packages: See JsInfo documentation
        transitive_sources: See JsInfo documentation

    Returns:
        A JsInfo provider
    """
    if type(declarations) != "depset":
        fail("Expected declarations to be a depset")
    if type(npm_linked_package_files) != "depset":
        fail("Expected npm_linked_package_files to be a depset")
    if type(npm_linked_packages) != "depset":
        fail("Expected npm_linked_packages to be a depset")
    if type(npm_package_store_deps) != "depset":
        fail("Expected npm_package_store_deps to be a depset")
    if type(sources) != "depset":
        fail("Expected sources to be a depset")
    if type(transitive_declarations) != "depset":
        fail("Expected transitive_declarations to be a depset")
    if type(transitive_npm_linked_package_files) != "depset":
        fail("Expected transitive_npm_linked_package_files to be a depset")
    if type(transitive_npm_linked_packages) != "depset":
        fail("Expected transitive_npm_linked_packages to be a depset")
    if type(transitive_sources) != "depset":
        fail("Expected transitive_sources to be a depset")

    return JsInfo(
        declarations = declarations,
        npm_linked_package_files = npm_linked_package_files,
        npm_linked_packages = npm_linked_packages,
        npm_package_store_deps = npm_package_store_deps,
        sources = sources,
        transitive_declarations = transitive_declarations,
        transitive_npm_linked_package_files = transitive_npm_linked_package_files,
        transitive_npm_linked_packages = transitive_npm_linked_packages,
        transitive_sources = transitive_sources,
    )
