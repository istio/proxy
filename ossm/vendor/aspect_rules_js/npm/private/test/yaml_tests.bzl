"Unit tests for yaml.bzl"

load("@bazel_skylib//lib:unittest.bzl", "asserts", "unittest")
load("//npm/private:yaml.bzl", "parse")

def _parse_basic_test_impl(ctx):
    env = unittest.begin(ctx)

    # Scalars
    asserts.equals(env, (True, None), parse("true"))
    asserts.equals(env, (False, None), parse("false"))
    asserts.equals(env, (1, None), parse("1"))
    asserts.equals(env, (3.14, None), parse("3.14"))
    asserts.equals(env, ("foo", None), parse("foo"))
    asserts.equals(env, ("foo", None), parse("'foo'"))
    asserts.equals(env, ("foo", None), parse("\"foo\""))
    asserts.equals(env, ("foo", None), parse("  foo"))
    asserts.equals(env, ("foo", None), parse("  foo  "))
    asserts.equals(env, ("foo", None), parse("foo  "))
    asserts.equals(env, ("foo", None), parse("\nfoo"))
    asserts.equals(env, ("foo", None), parse("foo\n"))
    asserts.equals(env, ("-foo", None), parse("-foo"))
    asserts.equals(env, ("foo{[]}", None), parse("foo{[]}"))

    return unittest.end(env)

def _parse_sequences_test_impl(ctx):
    env = unittest.begin(ctx)

    # Sequences (- notation)
    asserts.equals(env, (["foo"], None), parse("- foo"))
    asserts.equals(env, (["foo - bar"], None), parse("- foo - bar"))
    asserts.equals(env, (["foo", "bar"], None), parse("""\
- foo
- bar
"""))
    asserts.equals(env, (["foo"], None), parse("""\
-
    foo
"""))
    asserts.equals(env, (["foo", "bar"], None), parse("""\
-
    foo
-
    bar
"""))
    asserts.equals(env, (["foo", "bar"], None), parse("""\
- foo
-
    bar
"""))

    # Sequences ([] notation)
    asserts.equals(env, ([], None), parse("[]"))
    asserts.equals(env, (["foo"], None), parse("[foo]"))
    asserts.equals(env, (["fo o"], None), parse("[fo o]"))
    asserts.equals(env, (["fo\no"], None), parse("[fo\no]"))
    asserts.equals(env, (["foo", "bar"], None), parse("[foo,bar]"))
    asserts.equals(env, (["foo", "bar"], None), parse("[foo, bar]"))
    asserts.equals(env, ([1, True, "false"], None), parse("[1, true, \"false\"]"))
    asserts.equals(env, (["foo", "bar"], None), parse("""\
[
    foo,
    bar
]
"""))
    asserts.equals(env, (["foo", "bar"], None), parse("""\
[
    foo,
    bar,
]
"""))
    asserts.equals(env, (["foo", "bar"], None), parse("""\
[
    'foo',
    "bar",
]
"""))
    asserts.equals(env, (["foo", "bar"], None), parse("""\
[
    foo,

    bar
]
"""))
    asserts.equals(env, ([["foo", "bar"]], None), parse("[[foo,bar]]"))
    asserts.equals(env, ([["foo", [1, True]]], None), parse("[[foo,[1, true]]]"))
    asserts.equals(env, ([["foo", "bar"]], None), parse("[[foo, bar]]"))
    asserts.equals(env, ([["foo", "bar"]], None), parse("""\
[
    [
        foo,
        bar
    ]
]
"""))

    return unittest.end(env)

def _parse_maps_test_impl(ctx):
    env = unittest.begin(ctx)

    # Maps - scalar properties
    asserts.equals(env, ({"foo": "bar"}, None), parse("foo: bar"))
    asserts.equals(env, ({"foo": "bar"}, None), parse("foo: 'bar'"))
    asserts.equals(env, ({"foo": "bar"}, None), parse("foo: \"bar\""))
    asserts.equals(env, ({"foo": "bar"}, None), parse("foo: bar  "))
    asserts.equals(env, ({"foo": "bar"}, None), parse("'foo': bar"))
    asserts.equals(env, ({"foo": "bar"}, None), parse("\"foo\": bar"))
    asserts.equals(env, ({"foo": 1.5}, None), parse("foo: 1.5"))
    asserts.equals(env, ({"foo": True}, None), parse("foo: true"))
    asserts.equals(env, ({"foo": False}, None), parse("foo: false"))
    asserts.equals(env, ({"foo": "bar:baz"}, None), parse("foo: bar:baz"))

    # Maps - flow notation
    asserts.equals(env, ({}, None), parse("{}"))
    asserts.equals(env, ({"foo": "bar"}, None), parse("{foo: bar}"))
    asserts.equals(env, ({"foo": "bar"}, None), parse("{foo: 'bar'}"))
    asserts.equals(env, ({"foo": "bar"}, None), parse("{foo: \"bar\"}"))
    asserts.equals(env, ({"foo": "bar"}, None), parse("{foo: bar  }"))
    asserts.equals(env, ({"foo": "bar"}, None), parse("{'foo': bar}"))
    asserts.equals(env, ({"foo": "bar"}, None), parse("{\"foo\": bar}"))
    asserts.equals(env, ({"foo": 1.5}, None), parse("{foo: 1.5}"))
    asserts.equals(env, ({"foo": True}, None), parse("{foo: true}"))
    asserts.equals(env, ({"foo": False}, None), parse("{foo: false}"))
    asserts.equals(env, ({"foo": "bar:baz"}, None), parse("{foo: bar:baz}"))
    asserts.equals(env, ({"foo": {"bar": 5}}, None), parse("{foo: {bar: 5}}"))
    asserts.equals(env, ({"foo": 5, "bar": 6}, None), parse("{foo: 5, bar: 6}"))
    asserts.equals(env, ({"foo": 5, "bar": {"moo": "cow"}, "faz": "baz"}, None), parse("{foo: 5, bar: {moo: cow}, faz: baz}"))
    asserts.equals(env, ({"foo": {"bar": 5}}, None), parse("""\
{
    foo:
        {
            bar: 5
        }
}
"""))

    return unittest.end(env)

def _parse_multiline_test_impl(ctx):
    env = unittest.begin(ctx)

    # Literal multiline strings (|), strip (-) and keep (+)
    asserts.equals(env, ({"foo": "bar\n"}, None), parse("""\
foo: |
    bar
"""))
    asserts.equals(env, ({"foo": "bar\n", "moo": "cow\n"}, None), parse("""\
foo: |
    bar
moo: |
    cow
"""))
    asserts.equals(env, ({"foo": {"bar": "baz  \n faz\n"}}, None), parse("""\
foo:
    bar: |
     baz  
      faz
"""))
    asserts.equals(env, ({"a": "b\nc\nd\n"}, None), parse("""\
a: |
    b
    c
    d


"""))
    asserts.equals(env, ({"a": "\n\nb\n\nc\n\nd\n"}, None), parse("""\
a: |


    b

    c

    d


"""))
    asserts.equals(env, ({"foo": "bar"}, None), parse("""\
foo: |-
    bar
"""))
    asserts.equals(env, ({"foo": "bar", "moo": "cow"}, None), parse("""\
foo: |-
    bar
moo: |-
    cow
"""))
    asserts.equals(env, ({"foo": "bar\n"}, None), parse("""\
foo: |+
    bar
"""))
    asserts.equals(env, ({"a": "\n\nb\n\nc\n\nd\n\n\n"}, None), parse("""\
a: |+


    b

    c

    d


"""))
    asserts.equals(env, ({"foo": "bar\n", "moo": "cow", "faz": "baz\n\n"}, None), parse("""\
foo: |
    bar
moo: |-
    cow

faz: |+
    baz

"""))

    return unittest.end(env)

def _parse_mixed_test_impl(ctx):
    env = unittest.begin(ctx)

    # Mixed sequence and map flows
    asserts.equals(env, ({"foo": ["moo"]}, None), parse("{foo: [moo]}"))
    asserts.equals(env, (["foo", {"moo": "cow"}], None), parse("[foo, {moo: cow}]"))
    asserts.equals(env, ({"foo": {"moo": "cow", "faz": ["baz", 123]}}, None), parse("{foo: {moo: cow, faz: [baz, 123]}}"))
    asserts.equals(env, ([{"foo": ["bar", {"moo": ["cow"]}], "json": "bearded"}], None), parse("[{foo: [bar, {moo: [cow]}], json: bearded}]"))

    # Multi-level maps
    asserts.equals(env, ({"foo": {"moo": "cow"}}, None), parse("""\
foo:
    moo: cow
"""))
    asserts.equals(env, ({"foo": {"bar": {"moo": 5, "cow": True}}}, None), parse("""\
foo:
    bar:
        moo: 5
        cow: true
"""))
    asserts.equals(env, ({"a": {"b": {"c": {"d": 1, "e": 2, "f": 3}, "g": {"h": 4}}}}, None), parse("""\
a:
    b:
        c:
            d: 1
            e: 2
            f: 3
        g:
            h: 4
"""))

    # More than one root property
    asserts.equals(env, ({"a": True, "b": False}, None), parse("""\
a: true
b: false
"""))
    asserts.equals(env, ({"a": {"b": True}, "c": {"d": False}}, None), parse("""\
a:
    b: true
c:
    d: false
"""))

    # Value begins on next line at an indent
    asserts.equals(env, ({"moo": "cow"}, None), parse("""\
moo:
    cow
"""))

    # Mixed flow and non-flow maps
    asserts.equals(env, ({"foo": {"bar": {"moo": 5, "cow": True}}}, None), parse("""\
foo: {bar: {moo: 5, cow: true}}
"""))
    asserts.equals(env, ({"foo": {"bar": {"moo": 5, "cow": True}, "baz": "faz"}}, None), parse("""\
foo: {bar: {moo: 5, cow: true}, baz: faz}
"""))
    asserts.equals(env, ({"foo": {"bar": {"moo": 5, "cow": True}}}, None), parse("""\
foo:
    bar: {moo: 5, cow: true}
"""))
    asserts.equals(env, ({"foo": {"bar": {"moo": 5, "cow": True}, "baz": "faz"}, "json": ["bearded"]}, None), parse("""\
foo:
    bar:
        {
            moo: 5, cow: true
        }
    baz:
        faz
json: [bearded]
"""))
    asserts.equals(env, ({"foo": {"bar": {"moo": [{"cow": True}]}}}, None), parse("""\
foo:
    bar: {moo: [
            {cow: true}
        ]}
"""))

    # Miscellaneous
    asserts.equals(env, ({"foo": {"bar": "b-ar", "moo": ["cow"]}, "loo": ["roo", "goo"]}, None), parse("""\
foo:
    bar: b-ar
    moo: [cow]
loo:
    - roo
    - goo
"""))

    asserts.equals(env, ({"foo": "bar\n", "baz": 5}, None), parse("""\
foo: |
    bar
baz: 5
"""))

    return unittest.end(env)

def _parse_complex_map_test_impl(ctx):
    env = unittest.begin(ctx)

    # Basic complex-object
    asserts.equals(env, ({"a": True}, None), parse("""\
? a
: true
"""))

    # Multiple bsic complex-object
    asserts.equals(env, ({"a": True, "b": False}, None), parse("""\
? a
: true
? b
: false
"""))

    # Whitespace in various places
    asserts.equals(env, ({"a": True, "b": False, "c": "foo", "  d  ": "  e  "}, None), parse("""\
?   a
: true  
  
? b  
  
:   false  

? 'c'
:  foo  

? "  d  "  
:   "  e  "
"""))

    # Object in complex mapping key
    asserts.equals(env, ({"a": {"b": {"c": 1, "d": True}}}, None), parse("""\
a:
  ? b
  : c: 1
    d: true
"""))

    # Array in complex mapping key
    asserts.equals(env, ({"a": {"b": [1, True]}}, None), parse("""\
a:
  ? b
  : [1, true]
"""))

    # Arrays in complex mapping key
    asserts.equals(env, ({"a": [1, 2]}, None), parse("""\
? a
: - 1
  - 2
"""))

    # Multiple nesting, arrays in objects
    asserts.equals(env, ({"a": {"b": {"c": [1, 2]}}, "d": 3}, None), parse("""\
? a
: ? b
  : ? c
    : - 1
      - 2
? d
: 3
"""))

    # Popping in/out of nested maps
    asserts.equals(env, ({
        "a": {
            "b": {
                "c": 1,
                "d": {
                    "e": "f",
                },
                "g": 3,
            },
            "e": 3,
        },
    }, None), parse("""\
a:
  ? b
  : c: 1
    d:
       ? e
       : f
    g: 3
  ? e
  : 3
"""))

    return unittest.end(env)

def _parse_lockfile_test_impl(ctx):
    env = unittest.begin(ctx)

    # Partial lock file
    asserts.equals(env, ({
        "lockfileVersion": 5.4,
        "specifiers": {
            "@aspect-test/a": "5.0.0",
        },
        "dependencies": {
            "@aspect-test/a": "5.0.0",
        },
        "packages": {
            "/@aspect-test/a/5.0.0": {
                "resolution": {
                    "integrity": "sha512-t/lwpVXG/jmxTotGEsmjwuihC2Lvz/Iqt63o78SI3O5XallxtFp5j2WM2M6HwkFiii9I42KdlAF8B3plZMz0Fw==",
                },
                "hasBin": True,
                "dependencies": {
                    "@aspect-test/b": "5.0.0",
                    "@aspect-test/c": "1.0.0",
                    "@aspect-test/d": "2.0.0_@aspect-test+c@1.0.0",
                },
                "dev": False,
            },
        },
    }, None), parse("""\
lockfileVersion: 5.4

specifiers:
    '@aspect-test/a': 5.0.0

dependencies:
    '@aspect-test/a': 5.0.0

packages:

    /@aspect-test/a/5.0.0:
        resolution: {integrity: sha512-t/lwpVXG/jmxTotGEsmjwuihC2Lvz/Iqt63o78SI3O5XallxtFp5j2WM2M6HwkFiii9I42KdlAF8B3plZMz0Fw==}
        hasBin: true
        dependencies:
            '@aspect-test/b': 5.0.0
            '@aspect-test/c': 1.0.0
            '@aspect-test/d': 2.0.0_@aspect-test+c@1.0.0
        dev: false
"""))

    asserts.equals(env, ({
        "lockfileVersion": "6.0",
        "packages": {
            "/pkg-a@1.2.3": {
                "resolution": {
                    "integrity": "sha512-asdf",
                },
                "dev": False,
            },
            "/pkg-c@3.2.1": {
                "resolution": {
                    "integrity": "sha512-fdsa",
                },
                "dev": True,
            },
        },
    }, None), parse("""\
lockfileVersion: '6.0'

packages:
  ? /pkg-a@1.2.3
  : resolution: {integrity: sha512-asdf}
    dev: false

  ? /pkg-c@3.2.1
  : resolution: {integrity: sha512-fdsa}
    dev: true
"""))

    return unittest.end(env)

def _parse_conflict(ctx):
    env = unittest.begin(ctx)

    # Similar test to https://github.com/pnpm/pnpm/blob/37fffbefa5a9136f2e189c01a5edf3c00ac48018/packages/supi/test/lockfile.ts#L1237C1-L1249C13
    asserts.equals(env, (None, "Unknown result state: <<<<< HEAD for value 100.0.0"), parse("""\
importers:
  .:
    dependencies:
<<<<< HEAD
      dep-of-pkg-with-1-dep: 100.0.0
=====
      dep-of-pkg-with-1-dep: 100.1.0
>>>>> next
    specifiers:
      dep-of-pkg-with-1-dep: '>100.0.0'
lockfileVersion: 123
packages:
<<<<<<< HEAD
"""))

    return unittest.end(env)

def _parse_errors(ctx):
    env = unittest.begin(ctx)

    asserts.equals(env, (None, "Unknown result state: sdlkfdslkjf for value +"), parse("""\
asdljfk
            sdlkfdslkjf
    -
    +
    [-
  -- sldkjf
"""))

    asserts.equals(env, (None, "Unexpected EOF"), parse("""\
[asdljfk
"""))

    asserts.equals(env, (None, "Unexpected EOF"), parse("""\
a: [
"""))

    return unittest.end(env)

parse_basic_test = unittest.make(
    _parse_basic_test_impl,
    attrs = {},
)
parse_sequences_test = unittest.make(
    _parse_sequences_test_impl,
    attrs = {},
)
parse_maps_test = unittest.make(
    _parse_maps_test_impl,
    attrs = {},
)
parse_multiline_test = unittest.make(
    _parse_multiline_test_impl,
    attrs = {},
)

parse_mixed_test = unittest.make(
    _parse_mixed_test_impl,
    attrs = {},
)
parse_complex_map_test = unittest.make(
    _parse_complex_map_test_impl,
    attrs = {},
)
parse_lockfile_test = unittest.make(
    _parse_lockfile_test_impl,
    attrs = {},
)
parse_conflict_test = unittest.make(
    _parse_conflict,
    attrs = {},
)
parse_errors_test = unittest.make(
    _parse_errors,
    attrs = {},
)

def yaml_tests(name):
    unittest.suite(
        name,
        parse_basic_test,
        parse_sequences_test,
        parse_maps_test,
        parse_multiline_test,
        parse_mixed_test,
        parse_complex_map_test,
        parse_lockfile_test,
        parse_conflict_test,
        parse_errors_test,
    )
