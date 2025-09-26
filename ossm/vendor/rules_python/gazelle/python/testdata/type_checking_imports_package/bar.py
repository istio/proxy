from typing import TYPE_CHECKING

# foo should be added as a pyi_deps, since it is only imported in a type-checking context, but baz should be
# added as a deps.
from baz import X

if TYPE_CHECKING:
    import baz
    import foo
