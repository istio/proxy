"Unit tests for pnpm lock file parsing logic"

load("@bazel_skylib//lib:unittest.bzl", "asserts", "unittest")
load("//npm/private:utils.bzl", "utils")

def _parse_empty_lock_test_impl(ctx):
    env = unittest.begin(ctx)

    parsed_yaml = utils.parse_pnpm_lock_yaml("")
    parsed_json_a = utils.parse_pnpm_lock_json("")
    parsed_json_b = utils.parse_pnpm_lock_json("{}")
    expected = ({}, {}, {}, None)

    asserts.equals(env, expected, parsed_yaml)
    asserts.equals(env, expected, parsed_json_a)
    asserts.equals(env, expected, parsed_json_b)

    return unittest.end(env)

def _parse_merge_conflict_test_impl(ctx):
    env = unittest.begin(ctx)

    parsed = utils.parse_pnpm_lock_yaml("""
importers:
  .:
    dependencies:
<<<<< HEAD""")
    expected = ({}, {}, {}, "expected lockfileVersion key in lockfile")

    asserts.equals(env, expected, parsed)

    return unittest.end(env)

def _parse_lockfile_v5_test_impl(ctx):
    env = unittest.begin(ctx)

    parsed_yaml = utils.parse_pnpm_lock_yaml("""\
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
""")

    parsed_json = utils.parse_pnpm_lock_json("""\
{
  "lockfileVersion": 5.4,
  "specifiers": {
    "@aspect-test/a": "5.0.0"
  },
  "dependencies": {
    "@aspect-test/a": "5.0.0"
  },
  "packages": {
    "/@aspect-test/a/5.0.0": {
      "resolution": {
        "integrity": "sha512-t/lwpVXG/jmxTotGEsmjwuihC2Lvz/Iqt63o78SI3O5XallxtFp5j2WM2M6HwkFiii9I42KdlAF8B3plZMz0Fw=="
      },
      "hasBin": true,
      "dependencies": {
        "@aspect-test/b": "5.0.0",
        "@aspect-test/c": "1.0.0",
        "@aspect-test/d": "2.0.0_@aspect-test+c@1.0.0"
      },
      "dev": false
    }
  }
}
""")

    expected = (
        {
            ".": {
                "dependencies": {
                    "@aspect-test/a": "5.0.0",
                },
                "optionalDependencies": {},
                "devDependencies": {},
            },
        },
        {
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
        {},
        None,
    )

    asserts.equals(env, expected, parsed_yaml)
    asserts.equals(env, expected, parsed_json)

    return unittest.end(env)

def _parse_lockfile_v6_test_impl(ctx):
    env = unittest.begin(ctx)

    parsed_yaml = utils.parse_pnpm_lock_yaml("""\
lockfileVersion: '6.0'

dependencies:
  '@aspect-test/a':
    specifier: 5.0.0
    version: 5.0.0

packages:

  /@aspect-test/a@5.0.0:
    resolution: {integrity: sha512-t/lwpVXG/jmxTotGEsmjwuihC2Lvz/Iqt63o78SI3O5XallxtFp5j2WM2M6HwkFiii9I42KdlAF8B3plZMz0Fw==}
    hasBin: true
    dependencies:
      '@aspect-test/b': 5.0.0
      '@aspect-test/c': 1.0.0
      '@aspect-test/d': 2.0.0(@aspect-test/c@1.0.0)
    dev: false
""")

    parsed_json = utils.parse_pnpm_lock_json("""\
{
  "lockfileVersion": "6.0",
  "dependencies": {
    "@aspect-test/a": {
      "specifier": "5.0.0",
      "version": "5.0.0"
    }
  },
  "packages": {
    "/@aspect-test/a@5.0.0": {
      "resolution": {
        "integrity": "sha512-t/lwpVXG/jmxTotGEsmjwuihC2Lvz/Iqt63o78SI3O5XallxtFp5j2WM2M6HwkFiii9I42KdlAF8B3plZMz0Fw=="
      },
      "hasBin": true,
      "dependencies": {
        "@aspect-test/b": "5.0.0",
        "@aspect-test/c": "1.0.0",
        "@aspect-test/d": "2.0.0(@aspect-test/c@1.0.0)"
      },
      "dev": false
    }
  }
}
""")

    expected = (
        {
            ".": {
                "dependencies": {
                    "@aspect-test/a": "5.0.0",
                },
                "optionalDependencies": {},
                "devDependencies": {},
            },
        },
        {
            "/@aspect-test/a/5.0.0": {
                "resolution": {
                    "integrity": "sha512-t/lwpVXG/jmxTotGEsmjwuihC2Lvz/Iqt63o78SI3O5XallxtFp5j2WM2M6HwkFiii9I42KdlAF8B3plZMz0Fw==",
                },
                "hasBin": True,
                "dependencies": {
                    "@aspect-test/b": "5.0.0",
                    "@aspect-test/c": "1.0.0",
                    "@aspect-test/d": "2.0.0_at_aspect-test_c_1.0.0",
                },
                "dev": False,
                "optionalDependencies": {},
            },
        },
        {},
        None,
    )

    asserts.equals(env, expected, parsed_yaml)
    asserts.equals(env, expected, parsed_json)

    return unittest.end(env)

a_test = unittest.make(_parse_empty_lock_test_impl, attrs = {})
b_test = unittest.make(_parse_lockfile_v5_test_impl, attrs = {})
c_test = unittest.make(_parse_lockfile_v6_test_impl, attrs = {})
d_test = unittest.make(_parse_merge_conflict_test_impl, attrs = {})

TESTS = [
    a_test,
    b_test,
    c_test,
    d_test,
]

def parse_pnpm_lock_tests(name):
    for index, test_rule in enumerate(TESTS):
        test_rule(name = "{}_test_{}".format(name, index))
