"""Module doc for bzl_typedef."""

def _Square_typedef():
    """Represents a square

    :::{field} width
    :type: int
    The length of the sides
    :::

    """

def _Square_new(width):
    """Creates a square.

    Args:
        width: {type}`int` the side size

    Returns:
        {type}`Square`
    """

    # buildifier: disable=uninitialized
    self = struct(
        area = lambda *a, **k: _Square_area(self, *a, **k),
        width = width,
    )
    return self

def _Square_area(self):
    """Tells the area

    Args:
        self: implicitly added

    Returns:
        {type}`int`
    """
    return self.width * self.width

# buildifier: disable=name-conventions
Square = struct(
    TYPEDEF = _Square_typedef,
    new = _Square_new,
    area = _Square_area,
)
