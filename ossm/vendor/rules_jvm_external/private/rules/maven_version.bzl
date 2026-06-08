"""Maven ComparableVersion implementation in Starlark.

This implements the version comparison algorithm from Apache Maven's
ComparableVersion class. See:
https://maven.apache.org/pom.html#version-order-specification

Features:
- Mixing of '-' (hyphen) and '.' (dot) separators
- Transition between characters and digits constitutes a separator
- Unlimited number of version components
- Version components can be digits or strings
- Well-known qualifiers have special ordering (alpha < beta < milestone < rc < snapshot < "" < sp)
- Unknown qualifiers are sorted lexically after known qualifiers
"""

# Item type constants
_TYPE_INT = "int"
_TYPE_STR = "str"
_TYPE_LIST = "list"
_TYPE_COMBO = "combo"

# Qualifier ordering (from least to greatest)
# Index 5 ("") represents a release version
_QUALIFIERS = ["alpha", "beta", "milestone", "rc", "snapshot", "", "sp"]

# These qualifiers are equivalent to "" (release)
_RELEASE_QUALIFIERS = ["ga", "final", "release"]

# Aliases for qualifiers
_ALIASES = {"cr": "rc"}

def _strip_leading_zeroes(s):
    """Strip leading zeroes from a numeric string."""
    if not s:
        return "0"
    for i in range(len(s)):
        if s[i] != "0":
            return s[i:]
    return "0"

def _comparable_qualifier(qualifier):
    """
    Convert qualifier to a comparable string.

    Known qualifiers get their index, unknown qualifiers get "7-" prefix
    followed by the qualifier for lexical sorting after known ones.
    """
    if qualifier in _RELEASE_QUALIFIERS:
        return str(_QUALIFIERS.index(""))

    for i, q in enumerate(_QUALIFIERS):
        if q == qualifier:
            return str(i)

    # Unknown qualifier: sort after known ones, lexically
    return str(len(_QUALIFIERS)) + "-" + qualifier

def _new_int_item(value):
    """Create an integer item."""
    return {"type": _TYPE_INT, "value": int(value) if value else 0}

def _new_string_item(value, followed_by_digit):
    """Create a string item, handling single-char aliases."""
    if followed_by_digit and len(value) == 1:
        if value == "a":
            value = "alpha"
        elif value == "b":
            value = "beta"
        elif value == "m":
            value = "milestone"

    # Apply aliases (cr -> rc)
    value = _ALIASES.get(value, value)
    return {"type": _TYPE_STR, "value": value}

def _new_list_item(items):
    """Create a list item."""
    return {"type": _TYPE_LIST, "value": items}

def _new_combo_item(string_part, digit_part):
    """Create a combination item (string followed by digits, like 'alpha1')."""
    return {"type": _TYPE_COMBO, "string_part": string_part, "digit_part": digit_part}

def _parse_combination(buf):
    """Parse a combination token like 'alpha1' or 'rc2'."""
    buf = buf.replace("-", "")

    # Find where digits start
    index = len(buf)
    for i in range(len(buf)):
        if buf[i].isdigit():
            index = i
            break

    string_part = _new_string_item(buf[:index], True)
    digit_str = _strip_leading_zeroes(buf[index:]) if index < len(buf) else "0"
    digit_part = _new_int_item(digit_str)

    return _new_combo_item(string_part, digit_part)

def _item_is_null(item):
    """Check if an item is null (equivalent to empty/zero)."""
    t = item["type"]
    if t == _TYPE_INT:
        return item["value"] == 0
    elif t == _TYPE_STR:
        return not item["value"]
    elif t == _TYPE_LIST:
        return len(item["value"]) == 0
    elif t == _TYPE_COMBO:
        return False
    elif item["value"] == None:
        return True
    return False

def _normalize_list(items):
    """
    Normalize a list by removing trailing null items.

    This follows Maven's normalization rules:
    - Remove trailing null items (0 for numbers, "" for strings, empty lists)
    - But only if followed by a string or at the end
    """
    result = list(items)

    # Iterate backwards using range
    for idx in range(len(result) - 1, -1, -1):
        item = result[idx]
        if _item_is_null(item):
            next_idx = idx + 1
            should_remove = False

            if next_idx >= len(result):
                # At the end
                should_remove = True
            elif result[next_idx]["type"] == _TYPE_STR:
                # Followed by string
                should_remove = True
            elif result[next_idx]["type"] == _TYPE_LIST:
                # Followed by list - check first item in that list
                next_list = result[next_idx]["value"]
                if next_list:
                    first_type = next_list[0]["type"]
                    if first_type == _TYPE_COMBO or first_type == _TYPE_STR:
                        should_remove = True
                else:
                    should_remove = True

            if should_remove:
                result.pop(idx)

    return result

def _compare_strings(s1, s2):
    """Compare two strings lexicographically."""
    if s1 < s2:
        return -1
    elif s1 > s2:
        return 1
    return 0

def _compare_ints(i1, i2):
    """Compare two integers."""
    if i1 < i2:
        return -1
    elif i1 > i2:
        return 1
    return 0

def _compare_to_null_simple(item):
    """Compare a simple (non-list) item to null."""
    t = item["type"]

    if t == _TYPE_INT:
        return 0 if item["value"] == 0 else 1

    elif t == _TYPE_STR:
        release_idx = _comparable_qualifier("")
        return _compare_strings(_comparable_qualifier(item["value"]), release_idx)

    elif t == _TYPE_COMBO:
        # Combination's null comparison is based on its string part
        release_idx = _comparable_qualifier("")
        return _compare_strings(_comparable_qualifier(item["string_part"]["value"]), release_idx)

    return 0

def _compare_simple_items(left, right):
    """
    Compare two simple (non-list) items.

    Returns:
        -1 if left < right
        0 if left == right
        1 if left > right
    """
    lt = left["type"]
    rt = right["type"]

    # INT comparisons
    if lt == _TYPE_INT:
        if rt == _TYPE_INT:
            return _compare_ints(left["value"], right["value"])
        elif rt == _TYPE_STR:
            return 1  # number > string
        elif rt == _TYPE_COMBO:
            return 1  # number > combination
        elif rt == _TYPE_LIST:
            return 1  # number > list

        # STRING comparisons
    elif lt == _TYPE_STR:
        if rt == _TYPE_INT:
            return -1  # string < number
        elif rt == _TYPE_STR:
            q1 = _comparable_qualifier(left["value"])
            q2 = _comparable_qualifier(right["value"])
            return _compare_strings(q1, q2)
        elif rt == _TYPE_COMBO:
            # Compare string to combination's string part
            q1 = _comparable_qualifier(left["value"])
            q2 = _comparable_qualifier(right["string_part"]["value"])
            result = _compare_strings(q1, q2)
            if result == 0:
                return -1  # X < X1
            return result
        elif rt == _TYPE_LIST:
            return -1  # string < list

        # COMBINATION comparisons
    elif lt == _TYPE_COMBO:
        if rt == _TYPE_INT:
            return -1  # combination < number
        elif rt == _TYPE_STR:
            q1 = _comparable_qualifier(left["string_part"]["value"])
            q2 = _comparable_qualifier(right["value"])
            result = _compare_strings(q1, q2)
            if result == 0:
                return 1  # X1 > X
            return result
        elif rt == _TYPE_COMBO:
            q1 = _comparable_qualifier(left["string_part"]["value"])
            q2 = _comparable_qualifier(right["string_part"]["value"])
            result = _compare_strings(q1, q2)
            if result == 0:
                return _compare_ints(left["digit_part"]["value"], right["digit_part"]["value"])
            return result
        elif rt == _TYPE_LIST:
            return -1  # combination < list

        # LIST comparisons (comparing list to non-list)
    elif lt == _TYPE_LIST:
        if rt == _TYPE_INT:
            return -1  # list < number
        elif rt == _TYPE_STR:
            return 1  # list > string
        elif rt == _TYPE_COMBO:
            return 1  # list > combination

    return 0

def _compare_items(left, right):
    """
    Compare two items iteratively using a stack.

    Returns:
        -1 if left < right
        0 if left == right
        1 if left > right
    """

    # Stack entries: (left_item, right_item, list_index)
    # list_index is used when both items are lists to track position
    stack = [(left, right, 0)]

    # Process stack iteratively
    for _ in range(1000):  # Safety limit to prevent infinite loops
        if not stack:
            return 0

        current_left, current_right, idx = stack.pop()

        # Handle null comparisons
        if current_right == None:
            if current_left == None:
                continue

            t = current_left["type"]
            if t == _TYPE_LIST:
                items = current_left["value"]
                if not items:
                    continue

                # Push all items to compare to null (in reverse order so first is processed first)
                for i in range(len(items) - 1, -1, -1):
                    stack.append((items[i], None, 0))
                continue
            else:
                result = _compare_to_null_simple(current_left)
                if result != 0:
                    return result
                continue

        if current_left == None:
            # Invert comparison
            t = current_right["type"]
            if t == _TYPE_LIST:
                items = current_right["value"]
                if not items:
                    continue

                for i in range(len(items) - 1, -1, -1):
                    stack.append((None, items[i], 0))
                continue
            else:
                result = _compare_to_null_simple(current_right)
                if result != 0:
                    return -1 * result
                continue

        lt = current_left["type"]
        rt = current_right["type"]

        # Both are lists - compare element by element
        if lt == _TYPE_LIST and rt == _TYPE_LIST:
            left_items = current_left["value"]
            right_items = current_right["value"]
            max_len = max(len(left_items), len(right_items))

            # Push remaining comparisons in reverse order
            for i in range(max_len - 1, -1, -1):
                l = left_items[i] if i < len(left_items) else None
                r = right_items[i] if i < len(right_items) else None
                stack.append((l, r, 0))
            continue

        # At least one is not a list - compare directly
        result = _compare_simple_items(current_left, current_right)
        if result != 0:
            return result

    return 0

def _parse_version(version):
    """
    Parse a version string into a structured list.

    Args:
        version: The version string to parse

    Returns:
        A list item containing the parsed version structure
    """
    version = version.lower()

    items = []
    stack = [items]
    current_list = [items]  # Use list to allow mutation in nested function scope

    is_digit = [False]  # Use list to allow mutation
    is_combination = [False]
    start_index = [0]

    def add_token(end_index):
        """Add a token from start_index to end_index."""
        if end_index == start_index[0]:
            current_list[0].append(_new_int_item("0"))
        else:
            token = version[start_index[0]:end_index]
            if is_combination[0]:
                current_list[0].append(_parse_combination(token))
            elif is_digit[0]:
                current_list[0].append(_new_int_item(_strip_leading_zeroes(token)))
            else:
                current_list[0].append(_new_string_item(token, False))

    def start_new_sublist():
        """Start a new sub-list."""
        if current_list[0]:
            new_list = []
            current_list[0].append(_new_list_item(new_list))
            stack.append(new_list)
            current_list[0] = new_list

    # Process each character
    skip_next = [False]
    for i in range(len(version)):
        if skip_next[0]:
            skip_next[0] = False
            continue

        c = version[i]

        if c == ".":
            add_token(i)
            is_combination[0] = False
            start_index[0] = i + 1
            is_digit[0] = False

        elif c == "-":
            if i == start_index[0]:
                current_list[0].append(_new_int_item("0"))
            else:
                # Check for X-1 pattern (should be treated as X1 combination)
                if not is_digit[0] and i < len(version) - 1:
                    next_c = version[i + 1]
                    if next_c.isdigit():
                        is_combination[0] = True
                        skip_next[0] = False  # Don't skip, continue processing
                        continue

                token = version[start_index[0]:i]
                if is_combination[0]:
                    current_list[0].append(_parse_combination(token))
                elif is_digit[0]:
                    current_list[0].append(_new_int_item(_strip_leading_zeroes(token)))
                else:
                    current_list[0].append(_new_string_item(token, False))

            start_index[0] = i + 1

            # Hyphen creates a new sub-list
            start_new_sublist()

            is_combination[0] = False
            is_digit[0] = False

        elif c.isdigit():
            if not is_digit[0] and i > start_index[0]:
                # Transition from string to digit: X1
                is_combination[0] = True
                start_new_sublist()
            is_digit[0] = True

        else:
            # Non-digit character
            if is_digit[0] and i > start_index[0]:
                # Transition from digit to string
                token = version[start_index[0]:i]
                if is_combination[0]:
                    current_list[0].append(_parse_combination(token))
                else:
                    current_list[0].append(_new_int_item(_strip_leading_zeroes(token)))

                start_index[0] = i
                start_new_sublist()
                is_combination[0] = False

            is_digit[0] = False

    # Handle remaining token
    if len(version) > start_index[0]:
        # Treat .X as -X for any string qualifier X
        if not is_digit[0] and current_list[0]:
            start_new_sublist()

        token = version[start_index[0]:]
        if is_combination[0]:
            current_list[0].append(_parse_combination(token))
        elif is_digit[0]:
            current_list[0].append(_new_int_item(_strip_leading_zeroes(token)))
        else:
            current_list[0].append(_new_string_item(token, False))

    # Normalize all lists in reverse order (deepest first)
    for j in range(len(stack) - 1, -1, -1):
        lst = stack[j]
        normalized = _normalize_list(lst)

        # Clear and repopulate the list
        for _ in range(len(lst)):
            lst.pop()
        for item in normalized:
            lst.append(item)

    return _new_list_item(items)

def _simple_item_to_string(item):
    """Convert a simple (non-list) item to string."""
    t = item["type"]
    if t == _TYPE_INT:
        return str(item["value"])
    elif t == _TYPE_STR:
        return item["value"]
    elif t == _TYPE_COMBO:
        return item["string_part"]["value"] + str(item["digit_part"]["value"])
    return ""

def _item_to_canonical(item):
    """
    Convert an item to its canonical string representation (iterative).

    Args:
        item: The parsed item

    Returns:
        The canonical string representation
    """
    if item["type"] != _TYPE_LIST:
        return _simple_item_to_string(item)

    # For lists, we need to flatten and build the string iteratively
    # Use a work queue approach: process list items and build result
    result_parts = []

    # Stack of (items_list, current_index, is_first_in_parent)
    stack = [(item["value"], 0, True)]

    for _ in range(1000):  # Safety limit
        if not stack:
            break

        current_items, current_idx, is_first = stack[-1]

        if current_idx >= len(current_items):
            stack.pop()
            continue

        sub_item = current_items[current_idx]
        stack[-1] = (current_items, current_idx + 1, False)

        # Add separator if not first item
        if not is_first:
            if sub_item["type"] == _TYPE_LIST:
                result_parts.append("-")
            else:
                result_parts.append(".")

        if sub_item["type"] == _TYPE_LIST:
            # Push the sub-list onto the stack
            if sub_item["value"]:
                stack.append((sub_item["value"], 0, True))
        else:
            result_parts.append(_simple_item_to_string(sub_item))

    return "".join(result_parts)

def get_canonical(version):
    """
    Get the canonical (normalized) form of a version string.

    The canonical form normalizes the version by:
    - Removing trailing zero segments (1.0.0 -> 1)
    - Normalizing qualifiers (1-ga -> 1, 1a1 -> 1.alpha.1)
    - Lowercasing everything

    Args:
        version: The version string

    Returns:
        The canonical form of the version
    """
    parsed = _parse_version(version)
    return _item_to_canonical(parsed)

def compare_maven_versions(v1, v2):
    """
    Compare two Maven version strings.

    This implements Maven's ComparableVersion comparison algorithm.

    Args:
        v1: First version string
        v2: Second version string

    Returns:
        -1 if v1 < v2
        0 if v1 == v2
        1 if v1 > v2
    """
    parsed1 = _parse_version(v1)
    parsed2 = _parse_version(v2)
    return _compare_items(parsed1, parsed2)

def is_version_greater(v1, v2):
    """Check if v1 is greater than v2."""
    return compare_maven_versions(v1, v2) > 0

def is_version_less(v1, v2):
    """Check if v1 is less than v2."""
    return compare_maven_versions(v1, v2) < 0

def is_version_equal(v1, v2):
    return compare_maven_versions(v1, v2) == 0

def max_version(versions):
    """
    Return the maximum version from a list of version strings.

    Args:
        versions: List of version strings

    Returns:
        The maximum version string, or None if the list is empty
    """
    if not versions:
        return None

    result = versions[0]
    for v in versions[1:]:
        if compare_maven_versions(v, result) > 0:
            result = v
    return result

def min_version(versions):
    """
    Return the minimum version from a list of version strings.

    Args:
        versions: List of version strings

    Returns:
        The minimum version string, or None if the list is empty
    """
    if not versions:
        return None

    result = versions[0]
    for v in versions[1:]:
        if compare_maven_versions(v, result) < 0:
            result = v
    return result

def sort_versions(versions, reverse = False):
    """
    Sort a list of version strings.

    This uses a simple insertion sort since Starlark doesn't have a built-in
    sort with custom comparator.

    Args:
        versions: List of version strings
        reverse: If True, sort in descending order

    Returns:
        A new sorted list of version strings
    """
    result = list(versions)

    # Insertion sort using for loops
    for i in range(1, len(result)):
        key = result[i]

        # Find insertion point
        insert_pos = 0
        found = False
        for k in range(i - 1, -1, -1):
            cmp = compare_maven_versions(result[k], key)
            should_move = cmp > 0 if not reverse else cmp < 0
            if not should_move:
                insert_pos = k + 1
                found = True
                break

        if not found:
            insert_pos = 0

        # Shift elements and insert
        if insert_pos < i:
            for m in range(i, insert_pos, -1):
                result[m] = result[m - 1]
            result[insert_pos] = key

    return result
