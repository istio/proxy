# Copyright 2023 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"Implementation of PEP440 version string normalization"

def mkmethod(self, method):
    """Bind a struct as the first arg to a function.

    This is loosely equivalent to creating a bound method of a class.
    """
    return lambda *args, **kwargs: method(self, *args, **kwargs)

def _isdigit(token):
    return token.isdigit()

def _isalnum(token):
    return token.isalnum()

def _lower(token):
    # PEP 440: Case sensitivity
    return token.lower()

def _is(reference):
    """Predicate testing a token for equality with `reference`."""
    return lambda token: token == reference

def _is_not(reference):
    """Predicate testing a token for inequality with `reference`."""
    return lambda token: token != reference

def _in(reference):
    """Predicate testing if a token is in the list `reference`."""
    return lambda token: token in reference

def _ctx(start):
    """Creates a context, which is state for parsing (or sub-parsing)."""
    return {
        # The result value from parsing
        "norm": "",
        # Where in the parser's input string this context starts.
        "start": start,
    }

def _open_context(self):
    """Open an new parsing ctx.

    If the current parsing step succeeds, call self.accept().
    If the current parsing step fails, call self.discard() to
    go back to how it was before we opened a new ctx.

    Args:
      self: The normalizer.
    """
    self.contexts.append(_ctx(_context(self)["start"]))
    return self.contexts[-1]

def _accept(self, key = None):
    """Close the current ctx successfully and merge the results.

    Args:
        self: {type}`Parser}
        key: {type}`str | None` the key to store the result in
            the most recent context. If not set, the key is "norm".

    Returns:
        {type}`bool` always True
    """
    finished = self.contexts.pop()
    self.contexts[-1]["norm"] += finished["norm"]
    if key:
        self.contexts[-1][key] = finished["norm"]

    self.contexts[-1]["start"] = finished["start"]
    return True

def _context(self):
    return self.contexts[-1]

def _discard(self, key = None):
    self.contexts.pop()
    if key:
        self.contexts[-1][key] = ""
    return False

def _new(input):
    """Create a new parser

    Args:
        input: {type}`str` input to parse

    Returns:
        {type}`Parser` a struct for a parser object.
    """
    self = struct(
        input = input,
        contexts = [_ctx(0)],
    )

    public = struct(
        # methods: keep sorted
        accept = mkmethod(self, _accept),
        context = mkmethod(self, _context),
        discard = mkmethod(self, _discard),
        open_context = mkmethod(self, _open_context),

        # attributes: keep sorted
        input = self.input,
    )
    return public

def accept(parser, predicate, value):
    """If `predicate` matches the next token, accept the token.

    Accepting the token means adding it (according to `value`) to
    the running results maintained in ctx["norm"] and
    advancing the cursor in ctx["start"] to the next token in
    `version`.

    Args:
      parser: The normalizer.
      predicate: function taking a token and returning a boolean
        saying if we want to accept the token.
      value: the string to add if there's a match, or, if `value`
        is a function, the function to apply to the current token
        to get the string to add.

    Returns:
      whether a token was accepted.
    """

    ctx = parser.context()

    if ctx["start"] >= len(parser.input):
        return False

    token = parser.input[ctx["start"]]

    if predicate(token):
        if type(value) in ["function", "builtin_function_or_method"]:
            value = value(token)

        ctx["norm"] += value
        ctx["start"] += 1
        return True

    return False

def accept_placeholder(parser):
    """Accept a Bazel placeholder.

    Placeholders aren't actually part of PEP 440, but are used for
    stamping purposes. A placeholder might be
    ``{BUILD_TIMESTAMP}``, for instance. We'll accept these as
    they are, assuming they will expand to something that makes
    sense where they appear. Before the stamping has happened, a
    resulting wheel file name containing a placeholder will not
    actually be valid.

    Args:
      parser: The normalizer.

    Returns:
      whether a placeholder was accepted.
    """
    ctx = parser.open_context()

    if not accept(parser, _is("{"), str):
        return parser.discard()

    start = ctx["start"]
    for _ in range(start, len(parser.input) + 1):
        if not accept(parser, _is_not("}"), str):
            break

    if not accept(parser, _is("}"), str):
        return parser.discard()

    return parser.accept()

def accept_digits(parser):
    """Accept multiple digits (or placeholders), up to a non-digit/placeholder.

    Args:
      parser: The normalizer.

    Returns:
      whether some digits (or placeholders) were accepted.
    """

    ctx = parser.open_context()
    start = ctx["start"]

    for i in range(start, len(parser.input) + 1):
        if not accept(parser, _isdigit, str) and not accept_placeholder(parser):
            if i - start >= 1:
                if ctx["norm"].isdigit():
                    # PEP 440: Integer Normalization
                    ctx["norm"] = str(int(ctx["norm"]))
                return parser.accept()
            break

    return parser.discard()

def accept_string(parser, string, replacement):
    """Accept a `string` in the input. Output `replacement`.

    Args:
      parser: The normalizer.
      string: The string to search for in the parser input.
      replacement: The normalized string to use if the string was found.

    Returns:
      whether the string was accepted.
    """
    ctx = parser.open_context()

    for character in string.elems():
        if not accept(parser, _in([character, character.upper()]), ""):
            return parser.discard()

    ctx["norm"] = replacement

    return parser.accept()

def accept_alnum(parser):
    """Accept an alphanumeric sequence.

    Args:
      parser: The normalizer.

    Returns:
      whether an alphanumeric sequence was accepted.
    """

    ctx = parser.open_context()
    start = ctx["start"]

    for i in range(start, len(parser.input) + 1):
        if not accept(parser, _isalnum, _lower) and not accept_placeholder(parser):
            if i - start >= 1:
                return parser.accept()
            break

    return parser.discard()

def accept_dot_number(parser):
    """Accept a dot followed by digits.

    Args:
      parser: The normalizer.

    Returns:
      whether a dot+digits pair was accepted.
    """
    parser.open_context()

    if accept(parser, _is("."), ".") and accept_digits(parser):
        return parser.accept()
    else:
        return parser.discard()

def accept_dot_number_sequence(parser):
    """Accept a sequence of dot+digits.

    Args:
      parser: The normalizer.

    Returns:
      whether a sequence of dot+digits pairs was accepted.
    """
    ctx = parser.context()
    start = ctx["start"]
    i = start

    for i in range(start, len(parser.input) + 1):
        if not accept_dot_number(parser):
            break
    return i - start >= 1

def accept_separator_alnum(parser):
    """Accept a separator followed by an alphanumeric string.

    Args:
      parser: The normalizer.

    Returns:
      whether a separator and an alphanumeric string were accepted.
    """
    ctx = parser.open_context()

    # PEP 440: Local version segments
    if not accept(parser, _in([".", "-", "_"]), "."):
        return parser.discard()

    if accept_alnum(parser):
        # First character is separator; skip it.
        value = ctx["norm"][1:]

        # PEP 440: Integer Normalization
        if value.isdigit():
            value = str(int(value))
            ctx["norm"] = ctx["norm"][0] + value
        return parser.accept()

    return parser.discard()

def accept_separator_alnum_sequence(parser):
    """Accept a sequence of separator+alphanumeric.

    Args:
      parser: The normalizer.

    Returns:
      whether a sequence of separator+alphanumerics was accepted.
    """
    ctx = parser.context()
    start = ctx["start"]
    i = start

    for i in range(start, len(parser.input) + 1):
        if not accept_separator_alnum(parser):
            break

    return i - start >= 1

def accept_epoch(parser):
    """PEP 440: Version epochs.

    Args:
      parser: The normalizer.

    Returns:
      whether a PEP 440 epoch identifier was accepted.
    """
    ctx = parser.open_context()
    if accept_digits(parser) and accept(parser, _is("!"), "!"):
        if ctx["norm"] == "0!":
            ctx["norm"] = ""
        return parser.accept("epoch")
    else:
        return parser.discard("epoch")

def accept_release(parser):
    """Accept the release segment, numbers separated by dots.

    Args:
      parser: The normalizer.

    Returns:
      whether a release segment was accepted.
    """
    parser.open_context()

    if not accept_digits(parser):
        return parser.discard("release")

    accept_dot_number_sequence(parser)
    return parser.accept("release")

def accept_pre_l(parser):
    """PEP 440: Pre-release spelling.

    Args:
      parser: The normalizer.

    Returns:
      whether a prerelease keyword was accepted.
    """
    parser.open_context()

    if (
        accept_string(parser, "alpha", "a") or
        accept_string(parser, "a", "a") or
        accept_string(parser, "beta", "b") or
        accept_string(parser, "b", "b") or
        accept_string(parser, "c", "rc") or
        accept_string(parser, "preview", "rc") or
        accept_string(parser, "pre", "rc") or
        accept_string(parser, "rc", "rc")
    ):
        return parser.accept()
    else:
        return parser.discard()

def accept_prerelease(parser):
    """PEP 440: Pre-releases.

    Args:
      parser: The normalizer.

    Returns:
      whether a prerelease identifier was accepted.
    """
    ctx = parser.open_context()

    # PEP 440: Pre-release separators
    accept(parser, _in(["-", "_", "."]), "")

    if not accept_pre_l(parser):
        return parser.discard("pre")

    accept(parser, _in(["-", "_", "."]), "")

    if not accept_digits(parser):
        # PEP 440: Implicit pre-release number
        ctx["norm"] += "0"

    return parser.accept("pre")

def accept_implicit_postrelease(parser):
    """PEP 440: Implicit post releases.

    Args:
      parser: The normalizer.

    Returns:
      whether an implicit postrelease identifier was accepted.
    """
    ctx = parser.open_context()

    if accept(parser, _is("-"), "") and accept_digits(parser):
        ctx["norm"] = ".post" + ctx["norm"]
        return parser.accept()

    return parser.discard()

def accept_explicit_postrelease(parser):
    """PEP 440: Post-releases.

    Args:
      parser: The normalizer.

    Returns:
      whether an explicit postrelease identifier was accepted.
    """
    ctx = parser.open_context()

    # PEP 440: Post release separators
    if not accept(parser, _in(["-", "_", "."]), "."):
        ctx["norm"] += "."

    # PEP 440: Post release spelling
    if (
        accept_string(parser, "post", "post") or
        accept_string(parser, "rev", "post") or
        accept_string(parser, "r", "post")
    ):
        accept(parser, _in(["-", "_", "."]), "")

        if not accept_digits(parser):
            # PEP 440: Implicit post release number
            ctx["norm"] += "0"

        return parser.accept()

    return parser.discard()

def accept_postrelease(parser):
    """PEP 440: Post-releases.

    Args:
      parser: The normalizer.

    Returns:
      whether a postrelease identifier was accepted.
    """
    parser.open_context()

    if accept_implicit_postrelease(parser) or accept_explicit_postrelease(parser):
        return parser.accept("post")

    return parser.discard("post")

def accept_devrelease(parser):
    """PEP 440: Developmental releases.

    Args:
      parser: The normalizer.

    Returns:
      whether a developmental release identifier was accepted.
    """
    ctx = parser.open_context()

    # PEP 440: Development release separators
    if not accept(parser, _in(["-", "_", "."]), "."):
        ctx["norm"] += "."

    if accept_string(parser, "dev", "dev"):
        accept(parser, _in(["-", "_", "."]), "")

        if not accept_digits(parser):
            # PEP 440: Implicit development release number
            ctx["norm"] += "0"

        return parser.accept("dev")

    return parser.discard("dev")

def accept_local(parser):
    """PEP 440: Local version identifiers.

    Args:
      parser: The normalizer.

    Returns:
      whether a local version identifier was accepted.
    """
    parser.open_context()

    if accept(parser, _is("+"), "+") and accept_alnum(parser):
        accept_separator_alnum_sequence(parser)
        return parser.accept("local")

    return parser.discard("local")

def normalize_pep440(version):
    """Escape the version component of a filename.

    See https://packaging.python.org/en/latest/specifications/binary-distribution-format/#escaping-and-unicode
    and https://peps.python.org/pep-0440/

    Args:
      version: version string to be normalized according to PEP 440.

    Returns:
      string containing the normalized version.
    """
    return _parse(version, strict = True)["norm"]

def _parse(version_str, strict = True, _fail = fail):
    """Escape the version component of a filename.

    See https://packaging.python.org/en/latest/specifications/binary-distribution-format/#escaping-and-unicode
    and https://peps.python.org/pep-0440/

    Args:
      version_str: version string to be normalized according to PEP 440.
      strict: fail if the version is invalid, defaults to True.
      _fail: Used for tests

    Returns:
      string containing the normalized version.
    """

    # https://packaging.python.org/en/latest/specifications/version-specifiers/#leading-and-trailing-whitespace
    version = version_str.strip()
    is_prefix = False

    if not strict:
        is_prefix = version.endswith(".*")
        version = version.strip(" .*")  # PEP 440: Leading and Trailing Whitespace and ".*"

    parser = _new(version)
    accept(parser, _is("v"), "")  # PEP 440: Preceding v character
    accept_epoch(parser)
    accept_release(parser)
    accept_prerelease(parser)
    accept_postrelease(parser)
    accept_devrelease(parser)
    accept_local(parser)

    parser_ctx = parser.context()
    if parser.input[parser_ctx["start"]:]:
        if strict:
            _fail(
                "Failed to parse PEP 440 version identifier '%s'." % parser.input,
                "Parse error at '%s'" % parser.input[parser_ctx["start"]:],
            )

        return None

    parser_ctx["is_prefix"] = is_prefix
    return parser_ctx

def parse(version_str, strict = False, _fail = fail):
    """Parse a PEP4408 compliant version.

    This is similar to `normalize_pep440`, but it parses individual components to
    comparable types.

    Args:
      version_str: version string to be normalized according to PEP 440.
      strict: fail if the version is invalid.
      _fail: used for tests

    Returns:
      a struct with individual components of a version:
        * `epoch` {type}`int`, defaults to `0`
        * `release` {type}`tuple[int]` an n-tuple of ints
        * `pre` {type}`tuple[str, int] | None` a tuple of a string and an int,
            e.g. ("a", 1)
        * `post` {type}`tuple[str, int] | None` a tuple of a string and an int,
            e.g. ("~", 1)
        * `dev` {type}`tuple[str, int] | None` a tuple of a string and an int,
            e.g. ("", 1)
        * `local` {type}`tuple[str, int] | None` a tuple of components in the local
            version, e.g. ("abc", 123).
        * `is_prefix` {type}`bool` whether the version_str ends with `.*`.
        * `string` {type}`str` normalized value of the input.
    """

    parts = _parse(version_str, strict = strict, _fail = _fail)
    if not parts:
        return None

    if parts["is_prefix"] and (parts["local"] or parts["post"] or parts["dev"] or parts["pre"]):
        if strict:
            _fail("local version part has been obtained, but only public segments can have prefix matches")

        # https://peps.python.org/pep-0440/#public-version-identifiers
        return None

    return struct(
        epoch = _parse_epoch(parts["epoch"], _fail),
        release = _parse_release(parts["release"]),
        pre = _parse_pre(parts["pre"]),
        post = _parse_post(parts["post"], _fail),
        dev = _parse_dev(parts["dev"], _fail),
        local = _parse_local(parts["local"], _fail),
        string = parts["norm"],
        is_prefix = parts["is_prefix"],
    )

def _parse_epoch(value, fail):
    if not value:
        return 0

    if not value.endswith("!"):
        fail("epoch string segment needs to end with '!', got: {}".format(value))

    return int(value[:-1])

def _parse_release(value):
    return tuple([int(d) for d in value.split(".")])

def _parse_local(value, fail):
    if not value:
        return None

    if not value.startswith("+"):
        fail("local release identifier must start with '+', got: {}".format(value))

    # If the part is numerical, handle it as a number
    return tuple([int(part) if part.isdigit() else part for part in value[1:].split(".")])

def _parse_dev(value, fail):
    if not value:
        return None

    if not value.startswith(".dev"):
        fail("dev release identifier must start with '.dev', got: {}".format(value))
    dev = int(value[len(".dev"):])

    # Empty string goes first when comparing
    return ("", dev)

def _parse_pre(value):
    if not value:
        return None

    if value.startswith("rc"):
        prefix = "rc"
    else:
        prefix = value[0]

    return (prefix, int(value[len(prefix):]))

def _parse_post(value, fail):
    if not value:
        return None

    if not value.startswith(".post"):
        fail("post release identifier must start with '.post', got: {}".format(value))
    post = int(value[len(".post"):])

    # We choose `~` since almost all of the ASCII characters will be before
    # it. Use `ord` and `chr` functions to find a good value.
    return ("~", post)

def _pad_zeros(release, n):
    padding = n - len(release)
    if padding <= 0:
        return release

    release = list(release) + [0] * padding
    return tuple(release)

def _prefix_err(left, op, right):
    if left.is_prefix or right.is_prefix:
        fail("PEP440: only '==' and '!=' operators can use prefix matching: {} {} {}".format(
            left.string,
            op,
            right.string,
        ))

def _version_eeq(left, right):
    """=== operator"""
    if left.is_prefix or right.is_prefix:
        fail(_prefix_err(left, "===", right))

    # https://peps.python.org/pep-0440/#arbitrary-equality
    # > simple string equality operations
    return left.string == right.string

def _version_eq(left, right):
    """== operator"""
    if left.is_prefix and right.is_prefix:
        fail("Invalid comparison: both versions cannot be prefix matching")
    if left.is_prefix:
        return right.string.startswith("{}.".format(left.string))
    if right.is_prefix:
        return left.string.startswith("{}.".format(right.string))

    if left.epoch != right.epoch:
        return False

    release_len = max(len(left.release), len(right.release))
    left_release = _pad_zeros(left.release, release_len)
    right_release = _pad_zeros(right.release, release_len)

    if left_release != right_release:
        return False

    return (
        left.pre == right.pre and
        left.post == right.post and
        left.dev == right.dev
        # local is ignored for == checks
    )

def _version_compatible(left, right):
    """~= operator"""
    if left.is_prefix or right.is_prefix:
        fail(_prefix_err(left, "~=", right))

    # https://peps.python.org/pep-0440/#compatible-release
    # Note, the ~= operator can be also expressed as:
    # >= V.N, == V.*

    right_star = ".".join([str(d) for d in right.release[:-1]])
    if right.epoch:
        right_star = "{}!{}.".format(right.epoch, right_star)
    else:
        right_star = "{}.".format(right_star)

    return _version_ge(left, right) and left.string.startswith(right_star)

def _version_ne(left, right):
    """!= operator"""
    return not _version_eq(left, right)

def _version_lt(left, right):
    """< operator"""
    if left.is_prefix or right.is_prefix:
        fail(_prefix_err(left, "<", right))

    if left.epoch > right.epoch:
        return False
    elif left.epoch < right.epoch:
        return True

    release_len = max(len(left.release), len(right.release))
    left_release = _pad_zeros(left.release, release_len)
    right_release = _pad_zeros(right.release, release_len)

    if left_release > right_release:
        return False
    elif left_release < right_release:
        return True

    # From PEP440, this is not a simple ordering check and we need to check the version
    # semantically:
    # * The exclusive ordered comparison <V MUST NOT allow a pre-release of the specified version
    #   unless the specified version is itself a pre-release.
    if left.pre and right.pre:
        return left.pre < right.pre
    else:
        return False

def _version_gt(left, right):
    """> operator"""
    if left.is_prefix or right.is_prefix:
        fail(_prefix_err(left, ">", right))

    if left.epoch > right.epoch:
        return True
    elif left.epoch < right.epoch:
        return False

    release_len = max(len(left.release), len(right.release))
    left_release = _pad_zeros(left.release, release_len)
    right_release = _pad_zeros(right.release, release_len)

    if left_release > right_release:
        return True
    elif left_release < right_release:
        return False

    # From PEP440, this is not a simple ordering check and we need to check the version
    # semantically:
    # * The exclusive ordered comparison >V MUST NOT allow a post-release of the given version
    #   unless V itself is a post release.
    #
    # * The exclusive ordered comparison >V MUST NOT match a local version of the specified
    #   version.

    if left.post and right.post:
        return left.post > right.post
    else:
        # ignore the left.post if right is not a post if right is a post, then this evaluates to
        # False anyway.
        return False

def _version_le(left, right):
    """<= operator"""
    if left.is_prefix or right.is_prefix:
        fail(_prefix_err(left, "<=", right))

    # PEP440: simple order check
    # https://peps.python.org/pep-0440/#inclusive-ordered-comparison
    _left = _version_key(left, local = False)
    _right = _version_key(right, local = False)
    return _left < _right or _version_eq(left, right)

def _version_ge(left, right):
    """>= operator"""
    if left.is_prefix or right.is_prefix:
        fail(_prefix_err(left, ">=", right))

    # PEP440: simple order check
    # https://peps.python.org/pep-0440/#inclusive-ordered-comparison
    _left = _version_key(left, local = False)
    _right = _version_key(right, local = False)
    return _left > _right or _version_eq(left, right)

def _version_key(self, *, local = True):
    """This function returns a tuple that can be used in 'sorted' calls.

    This implements the PEP440 version sorting.
    """
    release_key = ("z",)
    local = self.local if local else []
    local = local or []

    return (
        self.epoch,
        self.release,
        # PEP440 Within a pre-release, post-release or development release segment with
        # a shared prefix, ordering MUST be by the value of the numeric component.
        # PEP440 release ordering: .devN, aN, bN, rcN, <no suffix>, .postN
        # We choose to first match the pre-release, then post release, then dev and
        # then stable
        self.pre or self.post or self.dev or release_key,
        # PEP440 local versions go before post versions
        tuple([(type(item) == "int", item) for item in local]),
        # PEP440 - pre-release ordering: .devN, <no suffix>, .postN
        self.post or self.dev or release_key,
        # PEP440 - post release ordering: .devN, <no suffix>
        self.dev or release_key,
    )

version = struct(
    normalize = normalize_pep440,
    parse = parse,
    # methods, keep sorted
    key = _version_key,
    is_compatible = _version_compatible,
    is_eq = _version_eq,
    is_eeq = _version_eeq,
    is_ge = _version_ge,
    is_gt = _version_gt,
    is_le = _version_le,
    is_lt = _version_lt,
    is_ne = _version_ne,
)
