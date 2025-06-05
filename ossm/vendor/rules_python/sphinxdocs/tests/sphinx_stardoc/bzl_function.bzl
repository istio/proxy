"""Tests for plain functions."""

def middle_varargs(a, *args, b):
    """Expect: `middle_varargs(a, *args, b)`

    NOTE: https://github.com/bazelbuild/stardoc/issues/226: `*args` renders last

    Args:
        a: {type}`str` doc for a
        *args: {type}`varags` doc for *args
        b: {type}`list[str]` doc for c

    """
    _ = a, args, b  # @unused

def mixture(a, b = 1, *args, c, d = 2, **kwargs):
    """Expect: `mixture(a, b=1, *args, c, d=2, **kwargs)`"""
    _ = a, b, args, c, d, kwargs  # @unused

def only_varargs(*args):
    """Expect: `only_varargs(*args)`"""
    _ = args  # @unused

def only_varkwargs(**kwargs):
    """Expect: `only_varkwargs(**kwargs)`"""
    _ = kwargs  # @unused

def unnamed_varargs(*, a = 1, b):
    """Expect: unnamed_varargs(*, a=1, b)"""
    _ = a, b  # @unused

def varargs_and_varkwargs(*args, **kwargs):
    """Expect: `varargs_and_varkwargs(*args, **kwargs)`"""
    _ = args, kwargs  # @unused
