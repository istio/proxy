# Annotation: Include Dep

Test that the Python gazelle annotation `# gazelle:include_dep` correctly adds dependences
to the generated target even if those dependencies are not imported by the Python module.

The root directory tests that all `py_*` targets will correctly include the additional
dependencies.

The `subpkg` directory tests that all `# gazlle:include_dep` annotations found in all source
files are included in the generated target (such as during `generation_mode package`).
