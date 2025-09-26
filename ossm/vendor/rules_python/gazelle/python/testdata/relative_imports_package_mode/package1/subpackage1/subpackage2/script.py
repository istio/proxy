from ...my_library import (
    some_function,
)  # Import path should be package1.my_library.some_function
from ...my_library.foo import (
    some_function,
)  # Import path should be package1.my_library.foo.some_function
from .library import (
    other_module,
)  # Import path should be package1.subpackage1.subpackage2.library.other_module
from .. import some_module  # Import path should be package1.subpackage1.some_module
from .. import some_function  # Import path should be package1.subpackage1.some_function
