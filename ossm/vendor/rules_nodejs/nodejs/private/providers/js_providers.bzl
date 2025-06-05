"Pass information about JavaScript modules between rules"

JSModuleInfo = provider(
    doc = """JavaScript files and sourcemaps.""",
    fields = {
        "direct_sources": "Depset of direct JavaScript files and sourcemaps",
        "sources": "Depset of direct and transitive JavaScript files and sourcemaps",
    },
)

def js_module_info(sources, deps = []):
    """Constructs a JSModuleInfo including all transitive sources from JSModuleInfo providers in a list of deps.

    Args:
        sources: direct JS files
        deps: other targets that provide JSModuleInfo, typically from the deps attribute

    Returns:
        a single JSModuleInfo.
    """
    transitive_depsets = [sources]
    for dep in deps:
        if JSModuleInfo in dep:
            transitive_depsets.append(dep[JSModuleInfo].sources)

    return JSModuleInfo(
        direct_sources = sources,
        sources = depset(transitive = transitive_depsets),
    )
