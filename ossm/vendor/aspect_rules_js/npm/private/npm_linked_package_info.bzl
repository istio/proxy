"NpmLinkedPackageInfo provider"

NpmLinkedPackageInfo = provider(
    doc = "Provides a linked npm package",
    fields = {
        "label": "the label of the npm_link_package_store target the created this provider",
        "link_package": "package that this npm package is directly linked at",
        "package": "name of this npm package",
        "version": "version of this npm package",
        "store_info": "the NpmPackageStoreInfo of the linked npm package store that is backing this link",
        "files": "depset of files that are part of the linked npm package",
        "transitive_files": "depset of the transitive files that are part of the linked npm package and its transitive deps",
    },
)
