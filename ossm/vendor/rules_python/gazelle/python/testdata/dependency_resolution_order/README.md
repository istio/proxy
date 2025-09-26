# Dependency resolution order

This asserts that the generator resolves the dependencies in the right order:

1. Explicit resolution via gazelle:resolve.
2. Third-party dependencies matching in the `modules_mapping.json`.
3. Indexed generated first-party dependencies.
