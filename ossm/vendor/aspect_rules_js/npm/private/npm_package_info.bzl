"NpmPackageInfo provider"

NpmPackageInfo = provider(
    doc = "Provides the output directory (TreeArtifact) of an npm package containing the packages sources along with the package name and version",
    fields = {
        "package": "name of this npm package",
        "version": "version of this npm package",
        "directory": "the directory (typically a TreeArtifact) that contains the package sources",
        "npm_package_store_deps": "A depset of NpmPackageStoreInfo providers from npm dependencies of the package and the packages's transitive deps to use as direct dependencies when linking with npm_link_package",
    },
)
