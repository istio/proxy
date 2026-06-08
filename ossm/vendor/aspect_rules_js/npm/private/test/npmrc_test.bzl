"""Tests for parse_npmrc utility function"""

load("@bazel_skylib//lib:unittest.bzl", "asserts", "unittest")
load("//npm/private:npmrc.bzl", "parse_npmrc")

def _npmrc_test(ctx, expected, content):
    env = unittest.begin(ctx)
    asserts.equals(env, dict(expected), parse_npmrc(content))
    return unittest.end(env)

def _basic(ctx):
    return _npmrc_test(ctx, [["a", "b"], ["c", ""]], """
        a=b
        c=
    """)

def _comments(ctx):
    return _npmrc_test(ctx, [["a", "1"], ["b", "2"]], """
        ; foo
        a=1 ;bar ; baz
        # baz
        b=2#f
        ;c=d
        # hash comment
    """)

def _whitespace(ctx):
    return _npmrc_test(ctx, [["a", "b"], ["c", "3"], ["d", ""]], """
        ; foo

        a = b ; 

        c = 3

        d =   
    """)

def _dupe(ctx):
    return _npmrc_test(ctx, [["a", "3"]], """
        a=1 ;
        a =2
        a = 3
    """)

def _sections(ctx):
    return _npmrc_test(ctx, [["a", "1"], ["b", "2"]], """
        [a]
        a=1
        [b]
        b=2
        [c=3]
        [d]
    """)

def _case_sensitivity(ctx):
    return _npmrc_test(ctx, [["a", "not_overriden"], ["A", "MixeD"], ["B", "UPPER"]], """
        a=not_overriden
        A=overriden
        A=MixeD
        B=UPPER
    """)

def _glob_characters(ctx):
    return _npmrc_test(ctx, [["stars", "*/*.npmrc"], ["dstar", "**/*/*.foo"], ["exts", "*.{foo,bar}"], ["protocol", "file://path/to/file.ext"]], """
        stars=*/*.npmrc
        dstar= **/*/*.foo
        exts =*.{foo,bar}
        protocol=file://path/to/file.ext
    """)

def _quotes(ctx):
    return _npmrc_test(ctx, [["a", " 1 "], ["b", "2"], ["c", ""], ["d", "4"], ["e", "\"5\""], ["f", "'6'"], ["registry", "https://url.com"]], """
        a=" 1 "
        b= "2" #dkjf
        c = "" ; "dkj;f'
        d = '4'
        e = '"5"'
        f = "'6'"
        registry = 'https://url.com'
    """)

basic_test = unittest.make(_basic)
comments_test = unittest.make(_comments)
whitespace_test = unittest.make(_whitespace)
dupe_test = unittest.make(_dupe)
sections_test = unittest.make(_sections)
case_sensitivity_test = unittest.make(_case_sensitivity)
glob_characters_test = unittest.make(_glob_characters)
_quotes_test = unittest.make(_quotes)

def npmrc_tests(name):
    unittest.suite(
        name,
        basic_test,
        comments_test,
        whitespace_test,
        dupe_test,
        sections_test,
        case_sensitivity_test,
        glob_characters_test,
        _quotes_test,
    )
