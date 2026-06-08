"""
A simple TOML parser implementation in native Starlark.

Current featureset status:

### Basic Types
- [x] **Strings**: Double quoted (`"string"`) with escape sequences
- [x] **Strings**: Single quoted literal strings (`'string'`)
- [ ] **Strings**: Unquoted bare values (not part of TOML spec)
- [x] **Numbers**: Basic integers (`42`, `-17`)
- [x] **Numbers**: Basic floats (`3.14`, `-2.5`)
- [ ] **Numbers**: Integer formats with underscores (`1_000`)
- [ ] **Numbers**: Hexadecimal (`0xFF`), octal (`0o755`), binary (`0b1010`)
- [ ] **Numbers**: Float scientific notation (`5e+22`)
- [ ] **Numbers**: Special float values (`inf`, `nan`)
- [x] **Booleans**: `true` and `false`
- [x] **Comments**: Full-line comments starting with `#`
- [x] **Comments**: Inline comments (comments after values)

### Structure
- [x] **Key-value pairs**: `key = value`
- [x] **Sections**: `[section]`
- [x] **Nested sections**: `[section.subsection]`
- [ ] **Quoted keys**: `"127.0.0.1" = "value"`
- [ ] **Dotted keys**: `physical.color = "orange"`

### String Features
- [x] **Basic escape sequences**: `\n`, `\t`, `\r`, `\\`, `\"`, `\'`
- [ ] **Unicode escape sequences**: `\\uXXXX`, `\\UXXXXXXXX`
- [ ] **Multi-line basic strings**: `""\"...\"""`
- [ ] **Multi-line literal strings**: `'''...'''`

### Collections
- [ ] **Arrays**: `[1, 2, 3]`
- [ ] **Inline tables**: `{key = "value"}`
- [ ] **Array of tables**: `[[table]]`

### Advanced Types
- [ ] **Date and time**: `1979-05-27T07:32:00Z`
- [ ] **Local date**: `1979-05-27`
- [ ] **Local time**: `07:32:00`
- [ ] **Local date-time**: `1979-05-27T07:32:00`
"""

JSON_DECODE_ERROR = struct()

def parse_toml(content):
    """
    Parse TOML content and return a dictionary.

    Args:
        content: String containing TOML content

    Returns:
        Dictionary representing the parsed TOML
    """
    result = {}
    current_section = result
    section_path = []

    lines = content.split("\n")

    for line_num, line in enumerate(lines, 1):
        line = line.strip()

        # Skip empty lines and comments
        if not line or line.startswith("#"):
            continue

        # Handle sections [section]
        if line.startswith("["):
            # Check for array of tables [[section]] - not supported
            if line.startswith("[["):
                fail("Array of tables ([[section]]) are not supported %s" % _parser_location(line_num))

            # Strip inline comments from section line
            section_line = _strip_inline_comment(line)
            if not section_line.endswith("]"):
                fail("Invalid section syntax %s: %s" % (_parser_location(line_num), line))

            section_name = section_line[1:-1].strip()
            if not section_name:
                fail("Empty section name %s" % _parser_location(line_num))

            # Create nested section if needed
            section_parts = section_name.split(".")
            current_section = result
            section_path = []

            for part in section_parts:
                # Validate section name characters
                if not _is_valid_key(part):
                    fail("Invalid section name characters at line %d: %s (section names must contain only alphanumeric characters, underscores, or hyphens)" % (line_num, part))

                section_path.append(part)
                if part not in current_section:
                    current_section[part] = {}
                current_section = current_section[part]
            continue

        # Handle key-value pairs
        if "=" in line:
            key, value = _parse_key_value(line, line_num, section_path)
            current_section[key] = value
        else:
            fail("Invalid syntax %s: %s" % (_parser_location(line_num, section_path), line))

    return result

def _parser_location(line_num, section_path = None):
    """
    Create a formatted location string for error messages.

    Args:
        line_num: The line number where the error occurred
        section_path: Optional list of section path components

    Returns:
        Formatted location string like "at line 5" or "at line 5 in section [database.connection]"
    """
    location = "at line %d" % line_num
    if section_path:
        location += " in section [%s]" % ".".join(section_path)
    return location

def _strip_inline_comment(value_str):
    """
    Strip inline comments from a value string, respecting quoted strings.

    Args:
        value_str: The value string that may contain an inline comment

    Returns:
        The value string with inline comment removed
    """
    if not value_str:
        return value_str

    # Track if we're inside a quoted string
    in_single_quote = False
    in_double_quote = False
    i = 0

    # Iterate through each character to find unquoted '#'
    for _ in value_str.elems():
        if i >= len(value_str):
            break

        char = value_str[i]

        # Handle escape sequences - skip the next character if we're in a quoted string
        if char == "\\" and (in_single_quote or in_double_quote) and i + 1 < len(value_str):
            i += 2  # Skip the escaped character
            continue

        # Handle quote state changes
        if char == '"' and not in_single_quote:
            in_double_quote = not in_double_quote
        elif char == "'" and not in_double_quote:
            in_single_quote = not in_single_quote
        elif char == "#" and not in_single_quote and not in_double_quote:
            # Found an unquoted '#' - this starts an inline comment
            return value_str[:i].rstrip()

        i += 1

    # No inline comment found
    return value_str

def _parse_key_value(line, line_num, section_path):
    """Parse a key-value pair from a line."""

    parts = line.split("=", 1)
    if len(parts) != 2:
        fail("Invalid key-value pair %s: %s" % (_parser_location(line_num, section_path), line))

    key = parts[0].strip()
    value_str = parts[1].strip()

    # Strip inline comments from the value
    value_str = _strip_inline_comment(value_str)

    # Validate key characters (only alphanumeric, underscore, and hyphen allowed)
    if not _is_valid_key(key):
        fail("Invalid key characters %s: %s (keys must be non-empty and contain only alphanumeric characters, underscores, or hyphens)" % (_parser_location(line_num, section_path), key))

    # Check for quoted keys (not supported)
    if (key.startswith('"') and key.endswith('"')) or (key.startswith("'") and key.endswith("'")):
        fail("Quoted keys are not supported %s: %s" % (_parser_location(line_num, section_path), key))

    # Check for dotted keys in key-value pairs (not supported)
    if "." in key:
        fail("Dotted keys in key-value pairs are not supported %s: %s" % (_parser_location(line_num, section_path), key))

    # Parse the value
    value = _parse_value(value_str, line_num, section_path)

    return key, value

def _is_valid_key(key):
    """
    Check if a key contains only valid characters.

    Valid characters are: alphanumeric (a-z, A-Z, 0-9), underscore (_), and hyphen (-).

    Args:
        key: The key string to validate

    Returns:
        True if key is valid, False otherwise
    """
    if not key:
        return False

    for char in key.elems():
        if not (char.isalnum() or char == "_" or char == "-"):
            return False

    return True

def _parse_value(value_str, line_num, section_path):
    """Parse a value string into appropriate type."""

    if not value_str:
        fail("Empty value %s" % _parser_location(line_num, section_path))

    # Check for unsupported features
    if value_str.startswith('"""') or value_str.startswith("'''"):
        fail("Multi-line strings (triple quotes) are not supported %s" % _parser_location(line_num, section_path))

    # Check for arrays
    if value_str.startswith("[") and value_str.endswith("]"):
        fail("Arrays are not supported %s" % _parser_location(line_num, section_path))

    # Check for inline tables
    if value_str.startswith("{") and value_str.endswith("}"):
        return _parse_inline_table(value_str, line_num, section_path)

    # Handle quoted strings
    if (value_str.startswith('"') and value_str.endswith('"')) or \
       (value_str.startswith("'") and value_str.endswith("'")):
        return _parse_string(value_str, line_num, section_path)

    # Handle booleans
    if value_str.lower() == "true":
        return True
    elif value_str.lower() == "false":
        return False

    # Check for unsupported number formats before parsing
    _check_unsupported_number_formats(value_str, line_num, section_path)

    # Handle numbers
    if _is_number(value_str):
        return _parse_number(value_str)

    # Check for date/time formats (basic detection)
    if _looks_like_datetime(value_str):
        fail("Date/time values are not supported %s" % _parser_location(line_num, section_path))

    fail("Invalid value %s: '%s'" % (_parser_location(line_num, section_path), value_str))

def _parse_inline_table(value_str, line_num, section_path):
    """Parse an inline table"""
    if not (value_str.startswith("{") and value_str.endswith("}")):
        fail("Invalid inline table %s: '%s'" % (_parser_location(line_num, section_path), value_str))

    result = {}

    # We can't recurse, but that's fine. We'll just loop, and we know
    # the maximum number of times we need to do that.
    i = 1
    end = len(value_str)
    for _ in range(end):
        (key, value, i) = _read_key_and_value(value_str, i, line_num, section_path)

        if key in result:
            fail("Duplicate key seen %s: '%s'" % (_parser_location(line_num, section_path), value_str))

        result[key] = value

        # Are we done yet?
        for idx in range(i, end):
            if value_str[idx] == "}":
                return result
            if value_str[idx] == ",":
                i = idx + 1
                break  #  Out of this inner loop

    return result

def _read_key_and_value(value_str, i, line_num, section_path):
    equals_idx = value_str.find("=", i)
    quote_char = '"'
    quote_idx = value_str.find(quote_char, i)
    if quote_idx == -1:
        quote_char = "'"
        quote_idx = value_str.find(quote_char, i)

    if equals_idx > quote_idx:
        fail("Invalid quoted string inline table %s: '%s'" % (_parser_location(line_num, section_path), value_str))
    key = value_str[i:equals_idx].strip()

    # Find the end of the string, assuming we don't have an escape sequence
    end_quote = value_str.find(quote_char, quote_idx + 1) + 1
    value = _parse_string(value_str[quote_idx:end_quote], line_num, section_path)

    return key, value, end_quote

def _parse_string(value_str, line_num, section_path):
    """Parse a quoted string."""

    if len(value_str) < 2:
        fail("Invalid string %s: '%s'" % (_parser_location(line_num, section_path), value_str))

    # Check if it's a literal string (single quotes) or basic string (double quotes)
    if value_str.startswith("'") and value_str.endswith("'"):
        # Literal string - no escaping allowed, return content as-is
        return value_str[1:-1]
    elif value_str.startswith('"') and value_str.endswith('"'):
        # Basic string - handle escape sequences
        # Double-quoted TOML strings have the same spec as JSON strings <3

        return json.decode(value_str)
    else:
        fail("Invalid string format %s: %'s'" % (_parser_location(line_num, section_path), value_str))

def _is_number(s):
    """Check if string represents a number."""
    if not s:
        return False

    # Handle negative numbers
    if s.startswith("-"):
        s = s[1:]
        if not s:
            return False

    # Check for float
    if "." in s:
        parts = s.split(".")
        if len(parts) != 2:
            return False
        return parts[0].isdigit() and parts[1].isdigit()

    return s.isdigit()

def _parse_number(s):
    """Parse a number string to int or float."""
    if "." in s:
        return float(s)
    return int(s)

def _check_unsupported_number_formats(value_str, line_num, section_path):
    """Check for unsupported number formats and fail with appropriate messages."""

    # Check for underscores in numbers
    if "_" in value_str and (_is_number(value_str.replace("_", "")) or value_str.replace("_", "").replace(".", "").isdigit()):
        fail("Numbers with underscores are not supported %s: %s" % (_parser_location(line_num, section_path), value_str))

    # Check for hexadecimal numbers
    if value_str.lower().startswith("0x"):
        fail("Hexadecimal numbers are not supported %s: %s" % (_parser_location(line_num, section_path), value_str))

    # Check for octal numbers
    if value_str.lower().startswith("0o"):
        fail("Octal numbers are not supported %s: %s" % (_parser_location(line_num, section_path), value_str))

    # Check for binary numbers
    if value_str.lower().startswith("0b"):
        fail("Binary numbers are not supported %s: %s" % (_parser_location(line_num, section_path), value_str))

    lower_val = value_str.lower()

    # Check for special float values
    if lower_val in ["inf", "+inf", "-inf", "nan", "+nan", "-nan"]:
        fail("Special float values (inf, nan) are not supported %s: %s" % (_parser_location(line_num, section_path), value_str))

    # Check for scientific notation
    if "e" in lower_val:
        fail("Scientific notation is not supported %s: %s" % (_parser_location(line_num, section_path), value_str))

def _looks_like_datetime(value_str):
    """Basic detection of datetime-like strings."""
    if len(value_str) < 8:
        return False

    # Check first 10 characters for date pattern "YYYY-MM-DD" (like "2023-12-25")
    if len(value_str) >= 10:
        first_10 = value_str[:10]
        if (first_10[4] == "-" and first_10[7] == "-" and
            first_10[:4].isdigit() and first_10[5:7].isdigit() and first_10[8:10].isdigit()):
            return True

    # Check first 8 characters for time pattern "HH:MM:SS"
    first_8 = value_str[:8]
    if (first_8[2] == ":" and first_8[5] == ":" and
        first_8[:2].isdigit() and first_8[3:5].isdigit() and first_8[6:8].isdigit()):
        return True

    return False

def format_toml(data, indent_level = 0, section_prefix = ""):
    """
    Format a dictionary back to TOML string.

    Args:
        data: Dictionary to format
        indent_level: Current indentation level
        section_prefix: Prefix for nested sections

    Returns:
        TOML formatted string
    """
    result = []
    indent = "  " * indent_level

    # First pass: handle simple key-value pairs
    for key, value in data.items():
        if type(value) == "dict":
            continue  # Handle sections in second pass

        formatted_value = _format_value(value)
        result.append("%s%s = %s" % (indent, key, formatted_value))

    # Second pass: handle sections
    for key, value in data.items():
        if type(value) == "dict":
            if result:  # Add blank line before section
                result.append("")

            # Build full section name
            if section_prefix:
                section_name = "%s.%s" % (section_prefix, key)
            else:
                section_name = key

            section_header = "[%s]" % section_name
            result.append(section_header)

            section_content = format_toml(value, indent_level, section_name)
            if section_content:
                result.append(section_content)

    return "\n".join(result)

def _format_value(value):
    """Format a value for TOML output."""
    if type(value) == "string":
        # According to TOML v1.0.0 specification, all string values must be quoted
        return '"%s"' % _escape_string(value)
    elif type(value) == "bool":
        return "true" if value else "false"
    elif type(value) == "int":
        return str(value)
    elif type(value) == "float":
        return str(value)
    else:
        return str(value)

def _escape_string(s):
    """Escape special characters in string."""
    result = ""
    for char in s.elems():
        if char == '"':
            result += '\\"'
        elif char == "\\":
            result += "\\\\"
        elif char == "\n":
            result += "\\n"
        elif char == "\t":
            result += "\\t"
        elif char == "\r":
            result += "\\r"
        else:
            result += char
    return result
