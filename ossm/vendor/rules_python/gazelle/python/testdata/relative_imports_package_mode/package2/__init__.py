from .library import add as _add
from .library import divide as _divide
from .library import multiply as _multiply
from .library import subtract as _subtract


def add(a, b):
    return _add(a, b)


def divide(a, b):
    return _divide(a, b)


def multiply(a, b):
    return _multiply(a, b)


def subtract(a, b):
    return _subtract(a, b)
