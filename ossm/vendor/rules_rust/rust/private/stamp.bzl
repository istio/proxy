"""A small utility module dedicated to detecting whether or not the `--stamp` flag is enabled"""

def is_stamping_enabled(ctx, attr):
    """Determine whether or not build stamping is enabled

    Args:
        ctx (ctx): The rule's context object
        attr (struct): A rule's struct of attributes (`ctx.attr`)

    Returns:
        bool: The stamp value
    """
    stamp_num = getattr(attr, "stamp", 0)
    if stamp_num == 1:
        return True
    elif stamp_num == 0:
        return False
    elif stamp_num == -1:
        return ctx.configuration.stamp_binaries()
    else:
        fail("Unexpected `stamp` value: {}".format(stamp_num))
