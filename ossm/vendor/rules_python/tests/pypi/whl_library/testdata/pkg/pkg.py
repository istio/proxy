try:
    import optional_dep

    WITH_EXTRAS = True
except ImportError:
    WITH_EXTRAS = False
