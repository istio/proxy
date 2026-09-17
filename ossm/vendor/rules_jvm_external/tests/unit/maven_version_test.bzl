"""Tests for Maven version comparison."""

load("@bazel_skylib//lib:unittest.bzl", "asserts", "unittest")
load("//private/rules:maven_version.bzl", "compare_maven_versions", "get_canonical", "is_version_equal", "is_version_greater", "is_version_less", "max_version", "min_version", "sort_versions")

def _check_versions_order(v1, v2):
    """Check that v1 < v2."""
    cmp = compare_maven_versions(v1, v2)
    if cmp >= 0:
        fail("Expected {} < {}, but got cmp = {}".format(v1, v2, cmp))

    cmp_rev = compare_maven_versions(v2, v1)
    if cmp_rev <= 0:
        fail("Expected {} > {}, but got cmp = {}".format(v2, v1, cmp_rev))

def _check_versions_equal(v1, v2):
    """Check that v1 == v2."""
    cmp = compare_maven_versions(v1, v2)
    if cmp != 0:
        fail("Expected {} == {}, but got cmp = {}".format(v1, v2, cmp))

    cmp_rev = compare_maven_versions(v2, v1)
    if cmp_rev != 0:
        fail("Expected {} == {}, but got cmp = {}".format(v2, v1, cmp_rev))

def _check_versions_array_order(versions):
    """Check that versions are in ascending order."""
    for i in range(1, len(versions)):
        for j in range(i, len(versions)):
            _check_versions_order(versions[i - 1], versions[j])

# Test version arrays from Java tests
_VERSIONS_QUALIFIER = [
    "1-alpha2snapshot",
    "1-alpha2",
    "1-alpha-123",
    "1-beta-2",
    "1-beta123",
    "1-m2",
    "1-m11",
    "1-rc",
    "1-cr2",
    "1-rc123",
    "1-SNAPSHOT",
    "1",
    "1-sp",
    "1-sp2",
    "1-sp123",
    "1-abc",
    "1-def",
    "1-pom-1",
    "1-1-snapshot",
    "1-1",
    "1-2",
    "1-123",
]

_VERSIONS_NUMBER = [
    "2.0",
    "2.0.a",
    "2-1",
    "2.0.2",
    "2.0.123",
    "2.1.0",
    "2.1-a",
    "2.1b",
    "2.1-c",
    "2.1-1",
    "2.1.0.1",
    "2.2",
    "2.123",
    "11.a2",
    "11.a11",
    "11.b2",
    "11.b11",
    "11.m2",
    "11.m11",
    "11",
    "11.a",
    "11b",
    "11c",
    "11m",
]

def _versions_qualifier_impl(ctx):
    env = unittest.begin(ctx)
    _check_versions_array_order(_VERSIONS_QUALIFIER)
    return unittest.end(env)

versions_qualifier_test = unittest.make(_versions_qualifier_impl)

def _versions_number_impl(ctx):
    env = unittest.begin(ctx)
    _check_versions_array_order(_VERSIONS_NUMBER)
    return unittest.end(env)

versions_number_test = unittest.make(_versions_number_impl)

def _versions_equal_impl(ctx):
    env = unittest.begin(ctx)

    _check_versions_equal("1", "1")
    _check_versions_equal("1", "1.0")
    _check_versions_equal("1", "1.0.0")
    _check_versions_equal("1.0", "1.0.0")
    _check_versions_equal("1", "1-0")
    _check_versions_equal("1", "1.0-0")
    _check_versions_equal("1.0", "1.0-0")

    # no separator between number and character
    _check_versions_equal("1a", "1-a")
    _check_versions_equal("1a", "1.0-a")
    _check_versions_equal("1a", "1.0.0-a")
    _check_versions_equal("1.0a", "1-a")
    _check_versions_equal("1.0.0a", "1-a")
    _check_versions_equal("1x", "1-x")
    _check_versions_equal("1x", "1.0-x")
    _check_versions_equal("1x", "1.0.0-x")
    _check_versions_equal("1.0x", "1-x")
    _check_versions_equal("1.0.0x", "1-x")

    # cr = rc
    _check_versions_equal("1cr", "1rc")

    # special "aliases" a, b and m for alpha, beta and milestone
    _check_versions_equal("1a1", "1-alpha-1")
    _check_versions_equal("1b2", "1-beta-2")
    _check_versions_equal("1m3", "1-milestone-3")

    # case insensitive
    _check_versions_equal("1X", "1x")
    _check_versions_equal("1A", "1a")
    _check_versions_equal("1B", "1b")
    _check_versions_equal("1M", "1m")
    _check_versions_equal("1Cr", "1Rc")
    _check_versions_equal("1cR", "1rC")
    _check_versions_equal("1m3", "1Milestone3")
    _check_versions_equal("1m3", "1MileStone3")
    _check_versions_equal("1m3", "1MILESTONE3")

    return unittest.end(env)

versions_equal_test = unittest.make(_versions_equal_impl)

def _versions_have_same_order_impl(ctx):
    env = unittest.begin(ctx)

    # These have same order (compare to 0) but may not be equal
    asserts.equals(env, 0, compare_maven_versions("1ga", "1"))
    asserts.equals(env, 0, compare_maven_versions("1release", "1"))
    asserts.equals(env, 0, compare_maven_versions("1final", "1"))
    asserts.equals(env, 0, compare_maven_versions("1Ga", "1"))
    asserts.equals(env, 0, compare_maven_versions("1GA", "1"))
    asserts.equals(env, 0, compare_maven_versions("1RELEASE", "1"))
    asserts.equals(env, 0, compare_maven_versions("1release", "1"))
    asserts.equals(env, 0, compare_maven_versions("1RELeaSE", "1"))
    asserts.equals(env, 0, compare_maven_versions("1Final", "1"))
    asserts.equals(env, 0, compare_maven_versions("1FinaL", "1"))
    asserts.equals(env, 0, compare_maven_versions("1FINAL", "1"))

    return unittest.end(env)

versions_have_same_order_test = unittest.make(_versions_have_same_order_impl)

def _version_comparing_impl(ctx):
    env = unittest.begin(ctx)

    _check_versions_order("1", "2")
    _check_versions_order("1.5", "2")
    _check_versions_order("1", "2.5")
    _check_versions_order("1.0", "1.1")
    _check_versions_order("1.1", "1.2")
    _check_versions_order("1.0.0", "1.1")
    _check_versions_order("1.0.1", "1.1")
    _check_versions_order("1.1", "1.2.0")

    _check_versions_order("1.0-alpha-1", "1.0")
    _check_versions_order("1.0-alpha-1", "1.0-alpha-2")
    _check_versions_order("1.0-alpha-1", "1.0-beta-1")

    _check_versions_order("1.0-beta-1", "1.0-SNAPSHOT")
    _check_versions_order("1.0-SNAPSHOT", "1.0")
    _check_versions_order("1.0-alpha-1-SNAPSHOT", "1.0-alpha-1")

    _check_versions_order("1.0", "1.0-1")
    _check_versions_order("1.0-1", "1.0-2")
    _check_versions_order("1.0.0", "1.0-1")

    _check_versions_order("2.0-1", "2.0.1")
    _check_versions_order("2.0.1-klm", "2.0.1-lmn")
    _check_versions_order("2.0.1", "2.0.1-xyz")
    _check_versions_order("2.0.1", "2.0.1-123")
    _check_versions_order("2.0.1-xyz", "2.0.1-123")

    return unittest.end(env)

version_comparing_test = unittest.make(_version_comparing_impl)

def _leading_zeroes_impl(ctx):
    env = unittest.begin(ctx)
    _check_versions_order("0.7", "2")
    _check_versions_order("0.2", "1.0.7")
    return unittest.end(env)

leading_zeroes_test = unittest.make(_leading_zeroes_impl)

def _mng5568_impl(ctx):
    """Test MNG-5568 edge case for transitive consistency."""
    env = unittest.begin(ctx)

    a = "6.1.0"
    b = "6.1.0rc3"
    c = "6.1H.5-beta"  # unusual version string, with 'H' in the middle

    _check_versions_order(b, a)  # classical
    _check_versions_order(b, c)  # now b < c
    _check_versions_order(a, c)

    return unittest.end(env)

mng5568_test = unittest.make(_mng5568_impl)

def _mng6572_impl(ctx):
    """Test MNG-6572 optimization with large numbers."""
    env = unittest.begin(ctx)

    a = "20190126.230843"  # resembles a SNAPSHOT
    b = "1234567890.12345"  # 10 digit number
    c = "123456789012345.1H.5-beta"  # 15 digit number
    d = "12345678901234567890.1H.5-beta"  # 20 digit number

    _check_versions_order(a, b)
    _check_versions_order(b, c)
    _check_versions_order(a, c)
    _check_versions_order(c, d)
    _check_versions_order(b, d)
    _check_versions_order(a, d)

    return unittest.end(env)

mng6572_test = unittest.make(_mng6572_impl)

def _version_equal_with_leading_zeroes_impl(ctx):
    """Test all versions are equal when starting with leading zeroes."""
    env = unittest.begin(ctx)

    versions = [
        "0000000000000000001",
        "000000000000000001",
        "00000000000000001",
        "0000000000000001",
        "000000000000001",
        "00000000000001",
        "0000000000001",
        "000000000001",
        "00000000001",
        "0000000001",
        "000000001",
        "00000001",
        "0000001",
        "000001",
        "00001",
        "0001",
        "001",
        "01",
        "1",
    ]

    for i in range(len(versions)):
        for j in range(i, len(versions)):
            _check_versions_equal(versions[i], versions[j])

    return unittest.end(env)

version_equal_with_leading_zeroes_test = unittest.make(_version_equal_with_leading_zeroes_impl)

def _version_zero_equal_with_leading_zeroes_impl(ctx):
    """Test all '0' versions are equal with leading zeroes."""
    env = unittest.begin(ctx)

    versions = [
        "0000000000000000000",
        "000000000000000000",
        "00000000000000000",
        "0000000000000000",
        "000000000000000",
        "00000000000000",
        "0000000000000",
        "000000000000",
        "00000000000",
        "0000000000",
        "000000000",
        "00000000",
        "0000000",
        "000000",
        "00000",
        "0000",
        "000",
        "00",
        "0",
    ]

    for i in range(len(versions)):
        for j in range(i, len(versions)):
            _check_versions_equal(versions[i], versions[j])

    return unittest.end(env)

version_zero_equal_with_leading_zeroes_test = unittest.make(_version_zero_equal_with_leading_zeroes_impl)

def _mng6964_impl(ctx):
    """Test MNG-6964 edge cases for qualifiers starting with '-0.'."""
    env = unittest.begin(ctx)

    a = "1-0.alpha"
    b = "1-0.beta"
    c = "1"

    _check_versions_order(a, c)  # a < c
    _check_versions_order(b, c)  # b < c
    _check_versions_order(a, b)  # a < b

    return unittest.end(env)

mng6964_test = unittest.make(_mng6964_impl)

def _locale_independent_impl(ctx):
    """Test case insensitivity works for all letters."""
    env = unittest.begin(ctx)
    _check_versions_equal("1-abcdefghijklmnopqrstuvwxyz", "1-ABCDEFGHIJKLMNOPQRSTUVWXYZ")
    return unittest.end(env)

locale_independent_test = unittest.make(_locale_independent_impl)

def _mng7644_impl(ctx):
    """Test MNG-7644: 1.0.0.X1 < 1.0.0-X2 for any string X."""
    env = unittest.begin(ctx)

    for x in ["abc", "alpha", "a", "beta", "b", "def", "milestone", "m", "RC"]:
        # 1.0.0.X1 < 1.0.0-X2 for any string x
        _check_versions_order("1.0.0." + x + "1", "1.0.0-" + x + "2")

        # 2.0.X == 2-X == 2.0.0.X for any string x
        _check_versions_equal("2-" + x, "2.0." + x)
        _check_versions_equal("2-" + x, "2.0.0." + x)
        _check_versions_equal("2.0." + x, "2.0.0." + x)

    return unittest.end(env)

mng7644_test = unittest.make(_mng7644_impl)

def _mng7714_impl(ctx):
    """Test MNG-7714: sp qualifier ordering with redhat suffix."""
    env = unittest.begin(ctx)

    f = "1.0.final-redhat"
    sp1 = "1.0-sp1-redhat"
    sp2 = "1.0-sp-1-redhat"
    sp3 = "1.0-sp.1-redhat"

    _check_versions_order(f, sp1)
    _check_versions_order(f, sp2)
    _check_versions_order(f, sp3)

    return unittest.end(env)

mng7714_test = unittest.make(_mng7714_impl)

def _helper_functions_impl(ctx):
    """Test helper functions like max_version, min_version, sort_versions."""
    env = unittest.begin(ctx)

    versions = ["1.0", "2.0", "1.5", "1.0-alpha", "2.0-SNAPSHOT"]

    asserts.equals(env, "2.0", max_version(versions))
    asserts.equals(env, "1.0-alpha", min_version(versions))

    sorted_versions = sort_versions(versions)
    asserts.equals(env, ["1.0-alpha", "1.0", "1.5", "2.0-SNAPSHOT", "2.0"], sorted_versions)

    sorted_desc = sort_versions(versions, reverse = True)
    asserts.equals(env, ["2.0", "2.0-SNAPSHOT", "1.5", "1.0", "1.0-alpha"], sorted_desc)

    asserts.true(env, is_version_greater("2.0", "1.0"))
    asserts.true(env, is_version_less("1.0", "2.0"))
    asserts.true(env, is_version_equal("1.0", "1.0.0"))

    return unittest.end(env)

helper_functions_test = unittest.make(_helper_functions_impl)

def _compare_digit_to_letter_impl(ctx):
    """Test that digits are greater than letters."""
    env = unittest.begin(ctx)

    asserts.true(env, compare_maven_versions("7", "J") > 0)
    asserts.true(env, compare_maven_versions("J", "7") < 0)
    asserts.true(env, compare_maven_versions("7", "c") > 0)
    asserts.true(env, compare_maven_versions("c", "7") < 0)

    return unittest.end(env)

compare_digit_to_letter_test = unittest.make(_compare_digit_to_letter_impl)

def _lexicographic_order_impl(ctx):
    """Test lexicographic ordering of unknown qualifiers."""
    env = unittest.begin(ctx)

    asserts.true(env, compare_maven_versions("zebra", "aardvark") > 0)
    asserts.true(env, compare_maven_versions("aardvark", "zebra") < 0)

    return unittest.end(env)

lexicographic_order_test = unittest.make(_lexicographic_order_impl)

def _case_insensitive_impl(ctx):
    """Test case insensitivity."""
    env = unittest.begin(ctx)

    asserts.equals(env, 0, compare_maven_versions("1.0.0-ALPHA1", "1.0.0-alpha1"))
    asserts.equals(env, 0, compare_maven_versions("1.0.0-alpha1", "1.0.0-ALPHA1"))
    asserts.equals(env, 0, compare_maven_versions("1.A", "1.a"))
    asserts.equals(env, 0, compare_maven_versions("1.a", "1.A"))

    return unittest.end(env)

case_insensitive_test = unittest.make(_case_insensitive_impl)

def _get_canonical_impl(ctx):
    """Test canonical form generation."""
    env = unittest.begin(ctx)

    # Basic normalization - trailing zeros removed
    asserts.equals(env, "1", get_canonical("1.0.0"))
    asserts.equals(env, "1", get_canonical("1.0"))
    asserts.equals(env, "1", get_canonical("1"))

    # Qualifiers normalized - note: a/b/m only expand to alpha/beta/milestone
    # when followed by a digit (like "a1" -> "alpha1")
    asserts.equals(env, "1-alpha", get_canonical("1.0-alpha"))
    asserts.equals(env, "1-a", get_canonical("1-a"))  # standalone 'a' stays as 'a'
    asserts.equals(env, "1-b", get_canonical("1-b"))  # standalone 'b' stays as 'b'
    asserts.equals(env, "1-m", get_canonical("1-m"))  # standalone 'm' stays as 'm'
    asserts.equals(env, "1-rc", get_canonical("1-cr"))  # cr -> rc alias always applies

    # Aliases expand when followed by digit
    asserts.equals(env, "1-alpha1", get_canonical("1-a1"))
    asserts.equals(env, "1-beta2", get_canonical("1-b2"))
    asserts.equals(env, "1-milestone3", get_canonical("1-m3"))

    # Case normalized to lowercase
    asserts.equals(env, "1-alpha", get_canonical("1-ALPHA"))
    asserts.equals(env, "1-beta", get_canonical("1-BETA"))

    # MNG-7700 test cases
    asserts.equals(env, "x", get_canonical("0.x"))
    asserts.equals(env, "x", get_canonical("0-x"))
    asserts.equals(env, "rc", get_canonical("0.rc"))
    asserts.equals(env, "0-1", get_canonical("0-1"))
    asserts.equals(env, "0.2", get_canonical("0.2"))

    # Canonical of canonical should equal itself
    for v in ["1.0-alpha", "2.3.4", "1-SNAPSHOT", "1.0.0-RC1"]:
        canonical = get_canonical(v)
        asserts.equals(env, canonical, get_canonical(canonical))

    return unittest.end(env)

get_canonical_test = unittest.make(_get_canonical_impl)

def maven_version_test_suite():
    unittest.suite(
        "maven_version_tests",
        versions_qualifier_test,
        versions_number_test,
        versions_equal_test,
        versions_have_same_order_test,
        version_comparing_test,
        leading_zeroes_test,
        mng5568_test,
        mng6572_test,
        version_equal_with_leading_zeroes_test,
        version_zero_equal_with_leading_zeroes_test,
        mng6964_test,
        locale_independent_test,
        mng7644_test,
        mng7714_test,
        helper_functions_test,
        compare_digit_to_letter_test,
        lexicographic_order_test,
        case_insensitive_test,
        get_canonical_test,
    )
