# Type Checking Imports

Test that the Python gazelle correctly handles type-only imports inside `if TYPE_CHECKING:` blocks.

Type-only imports should be added to the `pyi_deps` attribute instead of the regular `deps` attribute.
