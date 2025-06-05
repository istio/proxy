"Function definitions for working with lists"

def every(f, arr):
    """Check if every item of `arr` passes function `f`.

    Example:
      `every(lambda i: i.endswith(".js"), ["app.js", "lib.js"]) // True`

    Args:
      f: Function to execute on every item
      arr: List to iterate over

    Returns:
      True or False
    """
    return len(filter(f, arr)) == len(arr)

def filter(f, arr):
    """Filter a list `arr` by applying a function `f` to each item.

    Example:
      `filter(lambda i: i.endswith(".js"), ["app.ts", "app.js", "lib.ts", "lib.js"]) // ["app.js", "lib.js"]`

    Args:
      f: Function to execute on every item
      arr: List to iterate over

    Returns:
      A new list containing items that passed the filter function.
    """
    res = []
    for a in arr:
        if f(a):
            res.append(a)
    return res

def find(f, arr):
    """Find a particular item from list `arr` by a given function `f`.

    Unlike `pick`, the `find` method returns a tuple of the index and the value of first item passing by `f`.
    Furthermore `find` does not fail if no item passes `f`.
    In this case `(-1, None)` is returned.

    Args:
      f: Function to execute on every item
      arr: List to iterate over

    Returns:
      Tuple (index, item)
    """
    i = 0
    for a in arr:
        if f(a):
            return (i, a)
        i += 1
    return (-1, None)

def map(f, arr):
    """Apply a function `f` with each item of `arr` and return a new list.

    Example:
      `map(lambda i: i*2, [1, 2, 3]) // [2, 4, 6]`

    Args:
      f: Function to execute on every item
      arr: List to iterate over

    Returns:
      A new list with all mapped items.
    """
    return [f(a) for a in arr]

def once(f, arr):
    """Check if exactly one item in list `arr` passes the given function `f`.

    Args:
      f: Function to execute on every item
      arr: List to iterate over

    Returns:
      True or False
    """
    return len(filter(f, arr)) == 1

def pick(f, arr):
    """Pick a particular item in list `arr` by a given function `f`.

    Unlike `filter`, the `pick` method returns the first item _found_ by `f`.
    If no item has passed `f`, the function will _fail_.

    Args:
      f: Function to execute on every item
      arr: List to iterate over

    Returns:
      item
    """
    for a in arr:
        if f(a):
            return a
    fail("Could not find any item")

def some(f, arr):
    """Check if at least one item of `arr` passes function `f`.

    Example:
      `some(lambda i: i.endswith(".js"), ["app.js", "lib.ts"]) // True`

    Args:
      f: Function to execute on every item
      arr: List to iterate over

    Returns:
      True or False
    """
    for a in arr:
        if f(a):
            return True
    return False

def unique(arr):
    """Return a new list with unique items in it.

    Example:
      `unique(["foo", "bar", "foo", "baz"]) // ["foo", "bar", "baz"]`

    Args:
      arr: List to iterate over

    Returns:
      A new list with unique items
    """
    res = []
    for a in arr:
        if a not in res:
            res.append(a)
    return res
