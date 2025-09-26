""

load("@rules_testing//lib:analysis_test.bzl", "test_suite")
load("//python/private:version.bzl", "version")  # buildifier: disable=bzl-visibility

_tests = []

def _test_normalization(env):
    prefixes = ["v", "  v", " \t\r\nv"]
    epochs = {
        "": ["", "0!", "00!"],
        "1!": ["1!", "001!"],
        "200!": ["200!", "00200!"],
    }
    releases = {
        "0.1": ["0.1", "0.01"],
        "2023.7.19": ["2023.7.19", "2023.07.19"],
    }
    pres = {
        "": [""],
        "a0": ["a", ".a", "-ALPHA0", "_alpha0", ".a0"],
        "a4": ["alpha4", ".a04"],
        "b0": ["b", ".b", "-BETA0", "_beta0", ".b0"],
        "b5": ["beta05", ".b5"],
        "rc0": ["C", "_c0", "RC", "_rc0", "-preview_0"],
    }
    explicit_posts = {
        "": [""],
        ".post0": [],
        ".post1": [".post1", "-r1", "_rev1"],
    }
    implicit_posts = [[".post1", "-1"], [".post2", "-2"]]
    devs = {
        "": [""],
        ".dev0": ["dev", "-DEV", "_Dev-0"],
        ".dev9": ["DEV9", ".dev09", ".dev9"],
        ".dev{BUILD_TIMESTAMP}": [
            "-DEV{BUILD_TIMESTAMP}",
            "_dev_{BUILD_TIMESTAMP}",
        ],
    }
    locals = {
        "": [""],
        "+ubuntu.7": ["+Ubuntu_7", "+ubuntu-007"],
        "+ubuntu.r007": ["+Ubuntu_R007"],
    }
    epochs = [
        [normalized_epoch, input_epoch]
        for normalized_epoch, input_epochs in epochs.items()
        for input_epoch in input_epochs
    ]
    releases = [
        [normalized_release, input_release]
        for normalized_release, input_releases in releases.items()
        for input_release in input_releases
    ]
    pres = [
        [normalized_pre, input_pre]
        for normalized_pre, input_pres in pres.items()
        for input_pre in input_pres
    ]
    explicit_posts = [
        [normalized_post, input_post]
        for normalized_post, input_posts in explicit_posts.items()
        for input_post in input_posts
    ]
    pres_and_posts = [
        [normalized_pre + normalized_post, input_pre + input_post]
        for normalized_pre, input_pre in pres
        for normalized_post, input_post in explicit_posts
    ] + [
        [normalized_pre + normalized_post, input_pre + input_post]
        for normalized_pre, input_pre in pres
        for normalized_post, input_post in implicit_posts
        if input_pre == "" or input_pre[-1].isdigit()
    ]
    devs = [
        [normalized_dev, input_dev]
        for normalized_dev, input_devs in devs.items()
        for input_dev in input_devs
    ]
    locals = [
        [normalized_local, input_local]
        for normalized_local, input_locals in locals.items()
        for input_local in input_locals
    ]
    postfixes = ["", "  ", " \t\r\n"]
    i = 0
    for nepoch, iepoch in epochs:
        for nrelease, irelease in releases:
            for nprepost, iprepost in pres_and_posts:
                for ndev, idev in devs:
                    for nlocal, ilocal in locals:
                        prefix = prefixes[i % len(prefixes)]
                        postfix = postfixes[(i // len(prefixes)) % len(postfixes)]
                        env.expect.that_str(
                            version.normalize(
                                prefix + iepoch + irelease + iprepost +
                                idev + ilocal + postfix,
                            ),
                        ).equals(
                            nepoch + nrelease + nprepost + ndev + nlocal,
                        )
                        i += 1

_tests.append(_test_normalization)

def _test_normalize_local(env):
    # Verify a local with a [digit][non-digit] sequence parses ok
    in_str = "0.1.0+brt.9e"
    actual = version.normalize(in_str)
    env.expect.that_str(actual).equals(in_str)

_tests.append(_test_normalize_local)

def _test_ordering(env):
    want = [
        # Taken from https://peps.python.org/pep-0440/#summary-of-permitted-suffixes-and-relative-ordering
        "1.dev0",
        "1.0.dev456",
        "1.0a1",
        "1.0a2.dev456",
        "1.0a12.dev456",
        "1.0a12",
        "1.0b1.dev456",
        "1.0b1.dev457",
        "1.0b2",
        "1.0b2.post345.dev456",
        "1.0b2.post345.dev457",
        "1.0b2.post345",
        "1.0rc1.dev456",
        "1.0rc1",
        "1.0",
        "1.0+abc.5",
        "1.0+abc.7",
        "1.0+5",
        "1.0.post456.dev34",
        "1.0.post456",
        "1.0.15",
        "1.1.dev1",
        "1!0.1",
    ]

    for lower, higher in zip(want[:-1], want[1:]):
        lower = version.parse(lower, strict = True)
        higher = version.parse(higher, strict = True)

        lower_key = version.key(lower)
        higher_key = version.key(higher)

        if not lower_key < higher_key:
            env.fail("Expected '{}'.key() to be smaller than '{}'.key(), but got otherwise: {} > {}".format(
                lower.string,
                higher.string,
                lower_key,
                higher_key,
            ))

_tests.append(_test_ordering)

def version_test_suite(name):
    test_suite(
        name = name,
        basic_tests = _tests,
    )
