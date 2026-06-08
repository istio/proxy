"""Module containing definitions of all Prost providers."""

ProstProtoInfo = provider(
    doc = "Rust Prost provider info",
    fields = {
        "dep_variant_info": "DepVariantInfo: For the compiled Rust gencode (also covers its " +
                            "transitive dependencies)",
        "package_info": "File: A newline delimited file of `--extern_path` values for protoc.",
        "transitive_dep_infos": "depset[DepVariantInfo]: Transitive dependencies of the compiled crate.",
    },
)
