"Setup yq toolchain repositories and rules"

load(":repo_utils.bzl", "repo_utils")

# Platform names follow the platform naming convention in @aspect_bazel_lib//:lib/private/repo_utils.bzl
YQ_PLATFORMS = {
    "darwin_amd64": struct(
        compatible_with = [
            "@platforms//os:macos",
            "@platforms//cpu:x86_64",
        ],
    ),
    "darwin_arm64": struct(
        compatible_with = [
            "@platforms//os:macos",
            "@platforms//cpu:aarch64",
        ],
    ),
    "linux_amd64": struct(
        compatible_with = [
            "@platforms//os:linux",
            "@platforms//cpu:x86_64",
        ],
    ),
    "linux_arm64": struct(
        compatible_with = [
            "@platforms//os:linux",
            "@platforms//cpu:aarch64",
        ],
    ),
    "linux_s390x": struct(
        compatible_with = [
            "@platforms//os:linux",
            "@platforms//cpu:s390x",
        ],
    ),
    "linux_ppc64le": struct(
        compatible_with = [
            "@platforms//os:linux",
            "@platforms//cpu:ppc",
        ],
    ),
    "windows_amd64": struct(
        compatible_with = [
            "@platforms//os:windows",
            "@platforms//cpu:x86_64",
        ],
    ),
}

# Note: this is not the latest release, because it has significant breaking changes.
# See https://github.com/bazel-contrib/bazel-lib/pull/421
DEFAULT_YQ_VERSION = "4.25.2"

# https://github.com/mikefarah/yq/releases
#
# The integrity hashes can be automatically fetched for the latest yq release by running
# `tools/yq_mirror_release.sh`. To calculate for a specific release run
# `tools/yq_mirror_release.sh <release_version>`
#
# Alternatively, you can compute them manually by running
# `shasum -b -a 384 [downloaded file] | awk '{ print $1 }' | xxd -r -p | base64`
YQ_VERSIONS = {
    "4.33.3": {
        "darwin_amd64": "sha384-IJhMHD71yq+OR8AHFPfZr3XVpFlG2ZAfcexDKojtSLcCMV1pw0X2jza4qFUZiKEt",
        "darwin_arm64": "sha384-euQkz1Bu/dFuJoRgG4xIh9BhP2RvOceTDPSY8EzSIM5xykbMkwDNhr1PtCcUF5ye",
        "linux_amd64": "sha384-FxFu8W26rRzme76WNhyIfxdqoPOw19Ghjq7m1RwmwODdz1a88kCg32p+imQdjyWj",
        "linux_arm64": "sha384-F7U1tgKNNYFEpvinw/xR8ssPKO1ek6BlYOrcSWydSz8oJvVld6cRpvslvhsoMVxm",
        "linux_s390x": "sha384-CFXPW+fJZ/1Xo40JRNPUq50JeffybujfphqVgjIeAqHXkkQ6lClhEXw+vryUDAcT",
        "linux_ppc64le": "sha384-2gZP9kabZgq7W0h8Y765He90iWlW9fXGI9yNTWxcG9HsP+BPWCEaKSRRNogG7CyB",
        "windows_amd64": "sha384-39INMTRP1pJJSVaewmo6m0uW1Hr1+gl+YwhLjvWmnf398px/c2yEMxMqMroUGa+T",
    },
    "4.33.2": {
        "darwin_amd64": "sha384-XtZjB1INvy8sjKvqzNwMcrKrhg7C6NO4L3orOg1c6Md3jnphPI4KMZha0Lo24783",
        "darwin_arm64": "sha384-BKad2vcROUyJTHOVWXXIoalCJor4RaPz17BbzFz5o3S8m9fa1mmwOVIJGL8obKnP",
        "linux_amd64": "sha384-HPudnhx0IPFTDwjOJptaucSMWXolD3UkXzyBYbPBTPNgcNlRb0HKmM3zHvcqXXhe",
        "linux_arm64": "sha384-lP9wYHbqw84BWtbzB6vlFQUwtswjomUNyewSuq1vRP6KvK+cTtBM0dOuX2sB8sQk",
        "linux_s390x": "sha384-cFYDpNs4lZq5MIEawBv/wDAVSfq/k1NpB+K8ARSwGSGVQ6+gi/X8OqMw87vqNAxb",
        "linux_ppc64le": "sha384-MN72jQflMlMZ/5AZRDcpgejIaP4vo4gQlO7Z/eG9vsLVIVbSRXyyR9hisd3nQXvz",
        "windows_amd64": "sha384-WZg1jUKAF16o3WtWz3ks9kOIKb5KKW7SbO5dZo7O2xT4yuBRUIzsQ8uL00K4KKfE",
    },
    "4.33.1": {
        "darwin_amd64": "sha384-xzOpa6L3LvJOSTN9+VRf6sQKFPo13ZMNbDNhb05Dncoz9r6+2V/ytNh32OdulJMV",
        "darwin_arm64": "sha384-rFUnzTgVkMoU+Tvts1LKbMnaiaq1y69bKcPm2973jSxEm/AauNirbaJE4rcBm1C6",
        "linux_amd64": "sha384-KVzh5wvwStXkdQa2XjBLDs98Kmna/pAarh9ewdSUKVRMPo4LGa6JmqcoVC0JWaTa",
        "linux_arm64": "sha384-5ytwsHbS+POc7qehjvs5BUGerTHUNQiPYChQdDCxBD0lrWfonkEnEBkpV4TkGybQ",
        "linux_s390x": "sha384-hiiIiqBHlxrDL6Fy0vRDbNGgv3kriH7GPSlhGe6v6ccO+9Zqg2hH/nFnDORrh9vr",
        "linux_ppc64le": "sha384-fxnAtNDoLHemF4t7v/hMHVcR2fRetpIJ69Xn8J83I0E7Me7a6RabRhbMPT3ChfDq",
        "windows_amd64": "sha384-7V7vbTqFicN4qrXgvFw/hXgNx5PSgA5cj95l9200p56AkB4hhZypIexUmKe72Sct",
    },
    "4.32.2": {
        "darwin_amd64": "sha384-oujkjEXlsgQqadsFDoISMlh1CgITdU9yoZo1WChZhjWEmoSGAjtxxq1UbUn5xKXy",
        "darwin_arm64": "sha384-Z9vogzsLZFrZcNFqNJM1N4SGGQvIIfsEOTsuPBsSjQJUKtna8Tix3HGqT7IIP+y+",
        "linux_amd64": "sha384-Nh0XBd1SFo6bentP1bVZlPWkrPZA1AXNtOMjBtwr5zCLlnPgdy13JUcyXDX5ZErG",
        "linux_arm64": "sha384-8B06ZuVybfe4Ct16V7LPjwexIYZH6uENs57MO1ej7yKDspZZLNZFlnS1BK1W1Iq4",
        "linux_s390x": "sha384-hoIH6IBZ++9yYSWRV0w2eX2J9RJ1ezPJ4bAKMGkN0TNwWxLFr2zndVz00TxBLr/p",
        "linux_ppc64le": "sha384-mXvIwqUzP5YLU5dS1CCzkMsiM5jcq5JCbOwwFD2whCie+K07sEnKDuF6Kq3NH1Pv",
        "windows_amd64": "sha384-W8NchvmJR2/mH2e0WNQfRU+dy0BWk6lMRUWseb0Q+CwB8ML1YD66qrgKlSLI0UQL",
    },
    "4.32.1": {
        "darwin_amd64": "sha384-dC52sgUGWBCM6g+NHqGM6lGuWH8D43wDCycIRiP5fA0z0KfekgOKMexhoiIofw8u",
        "darwin_arm64": "sha384-GMClI5c7R8I3GcA/gCDH7klGOjytDYMkGmDxkk3+nM5sxmGyX73kyw1pzIPqlstN",
        "linux_amd64": "sha384-fGfnoAhqirivPO8Zu6Yik4iA/Y6Qg9R1GPjTQjMdJ70OTz2eIWKa66j36uuZiSXV",
        "linux_arm64": "sha384-tEJdFobIyINNCDHDlWApK4AY5aF1T+8zCZjka9HJzQfIl/JdJHy6cdx5GCbOVy8l",
        "linux_s390x": "sha384-KETACRoNt5F/2afheRNfbXP02JkzDtBuxkXJ65VOEx842z+wpBFGNMBhTNsXJJWY",
        "linux_ppc64le": "sha384-y0lz6WzI+w1M+TBpLKqLzkykz44kcK2z1wHkZCBWiriAUOrRW7jGUIp8/lkShB6m",
        "windows_amd64": "sha384-ez5I08/Lp0uXpBox8KtlighDChdOWzmIkQ+jXSu5Q0Vo67+v5NhVfpQziwEhj7is",
    },
    "4.31.2": {
        "darwin_amd64": "sha384-pLTjWopl7zvRZDWNmoVqYFFH5t8v9WMlQeH8DLdv0WIv8X9+zg4/FxSii144x3Tl",
        "darwin_arm64": "sha384-Yac/bbE1Vtg+o4B17UKZAWMBc4rRK4izItFdBL/fPT+Pcy0lH8m5lL6ozT74mdE0",
        "linux_amd64": "sha384-pqr5rrKsELcBvn+FPEYyH9gBRuhGD+n59d2IckdkStAjkK5QTDdXKDxY4r7Gstrl",
        "linux_arm64": "sha384-sdLKk5iGvJRYOkpjhOjneIn+Q0RDcqTKzaq3mmilcAy9MrBCNgKEBusLaNnV9/vP",
        "linux_s390x": "sha384-6bTlnAXkocRjBq3FAJy9xN6+H2TJsJu9NAaQxPPpbNQba+y/KqUTfIFAJuKm9LjO",
        "linux_ppc64le": "sha384-3Nn0eohG4XCB9JHM6mXirwe9z0AH7Ic/tsvcYxG4OAWs/U3vZuZlIp6f2Ts3kOGv",
        "windows_amd64": "sha384-Qy+JKO2iHuZxuOnq0ygxUa4mXdVflTr6URIP3dldGIgX+g4azdJC6I3Bvc3+qQNb",
    },
    "4.31.1": {
        "darwin_amd64": "sha384-k9Ftk7RfXjbFq8jMRXXH3nRAsGYPFV+aPpbDKtq33Kj59t25IkGxypR5bUKimIe2",
        "darwin_arm64": "sha384-6fLNJ+IuuCODGyIbObRgApWo/WakANSIEymoKeR4kt6mqIq0CVEbSJoBiGjIMMw6",
        "linux_amd64": "sha384-kX4ued7s19fNm6yYyZBiOG+UFb7yh65L8L64BkzvPNwAMAwQ+VgPh9lOySacgT7/",
        "linux_arm64": "sha384-EZrQjrlQODWsm6B8q36sbe7gvcNHNWQEgfRP8AsAvXS5PsD82sC17AeRvMbArAT4",
        "linux_s390x": "sha384-FvS4xs7A1e8Rom85xGCz8ldKTibqTMgN9WEAWIZ5ZjoJIXdTVqAUvPVNC56TEm18",
        "linux_ppc64le": "sha384-3mBcDXHCaG9+oDfmrrCG74UAulD5rsh0wl6FjFgN26/pSxUZ7nNtzqi0Ck4prx2f",
        "windows_amd64": "sha384-hEuszAhbPMkGL5ih2EwHfDedrhtmpT8WyQMZYLXzc0Fs5Nl90EqWXJ12wiYrTCz8",
    },
    "4.30.8": {
        "darwin_amd64": "sha384-cr734pTKzXdnhDJaBGgsG9stpihJKh+QyJHfnHYWhALyg/cPfVdEETkLhtj0qyiG",
        "darwin_arm64": "sha384-SBAt6LTJ//Qut9daSZ/4rg4BqspZR68AW15uBO1z8SYMnrVZERPjwIxnOgvaYpzF",
        "linux_amd64": "sha384-ym4aPXQZw8WaRV4utRzcPwEyqbKv0hUv0JhQqC0qs4HgaRm7r/CtwzUF8AOJ4/ys",
        "linux_arm64": "sha384-dDIUBKY9O6mYZZ+VXClt0jXzxSCC0XAtDp4qh9UXUGmySrQ6NbQlWBkeGYEAnQba",
        "linux_s390x": "sha384-jYgpByc5Li9GSfwKtQJwty7/INBT9k9TrHzaiBZ2dH0ptroZJLy4eqBy8n1LB1eA",
        "linux_ppc64le": "sha384-i/+d6ws6MjzNr+uFWa9Ca/GDKlH24hRuDaeYNWyn4qvvoGJQCMk8yYyDlvlJzUOV",
        "windows_amd64": "sha384-zcS0scnyOmYME8VL1OP5fIFhUzUgSWYR+0QWdh6QykpGEMWUjHkkAYWD03R8lXgK",
    },
    "4.30.7": {
        "darwin_amd64": "sha384-tUt6MQaK08R8BtAiHZa0mWlNPRnOm2LeyBAWN3v2JRolOJ8+gVA4fvhVgvawxx06",
        "darwin_arm64": "sha384-WWTrnfPjGhJhmoeOhPpcsIGVvmPPfhXqDE7wYhnV577qo7Iy0YDqprqg064JwfVQ",
        "linux_amd64": "sha384-6peNflw6HogPErPTmD2Fxd0IrPYwtBGRd8fag7Jgi6JmWSv4G9AyYVrRquWXw55J",
        "linux_arm64": "sha384-Bhlp2CZ363kAarj1GZ/Fj+nV8ISKHPqCInUNu9qBvBdlGSsZ16NYTN9HEn/BWQnk",
        "linux_s390x": "sha384-kPYUc9sn4gRfkr7uAiFcOElmBggUiF2sl8sgG0oiudysGWw87DxNBUWXkWRQqkDj",
        "linux_ppc64le": "sha384-kzC2vlWrmINddlsd/BYzhuUUfiZnmIDJi+WftwcPB+IWPEFpeYkCuK1bNO3nLI8s",
        "windows_amd64": "sha384-NON5Qd2wE+2ilmJ+j7J5Vc4EYKX+kTHGwLQJvpqa8Xf+BfwPJA389mSYk5mWtX2U",
    },
    "4.30.6": {
        "darwin_amd64": "sha384-I8oY8+z38GJMzaZDzxQ9FA2mOE9AkgZGs6JnP5wnjcGX85zbjjRdrMReaP6ZFTli",
        "darwin_arm64": "sha384-dYF6a2CQ6rl5zXBJaAtjg35cDRgYL164FTz/R1321REZHnJu/XqdVso5D10yLsb9",
        "linux_amd64": "sha384-yJAe6Vpsy0cz6G2D7nXYlqVlXcJjn53/0nlQWe67JgCfBpb0XnQOA6FuGsW6FttH",
        "linux_arm64": "sha384-QeC9CZNqy0iPu5LKSWcGuNW7Kk9UBjw+/VxnO4mlbJ9EHJRllT0QvTVoLHwhuCev",
        "linux_s390x": "sha384-ZPfwmcPczB/SAtcPtW3Qp1HvpeA45V6sjtiPAh9O0x33VnC/m0kHm4g4zJvILTza",
        "linux_ppc64le": "sha384-iKDFd83tC3LPxqcyge4b5TuETeAVdgYXDcuGDXNC2Rt6vhZ6syT0CJFtwm8bvl19",
        "windows_amd64": "sha384-zCwrdoR8bRBzvhQuUHCWHlgemOPNZOWxH2Gci+JIk55CUN48T4ES4kX6D/jNIzOg",
    },
    "4.30.5": {
        "darwin_amd64": "sha384-s63cFkXgchNM3CMZWGtlQ87mMd7x5ZkzbClQAOYV7lXCi1XSTnCxSccNJByvZBWW",
        "darwin_arm64": "sha384-hc6DFV3lgjUS13KzkfXP2DvdUPruT50//7skgXZUCuWoAro84r+VNOg4gt4AlFw+",
        "linux_amd64": "sha384-jHiMhF4qj1cVVWK8pGYhza8AcHKvtXlQG3kQZS+iziZmPf0GLQ08bAzWvYeKe8M/",
        "linux_arm64": "sha384-nk5G36aNWtlg/pSqF9GnfMVLkz2VYM3QgIJlTrLIVkFnO3ygp3BS0r+Wj8DVxXZ1",
        "linux_s390x": "sha384-UATZYKNkBNXWUVs7/rv9cU+yPlMPAr+p4fiwDDGwrTmj7QnxKuQ9XDvw0rryied3",
        "linux_ppc64le": "sha384-KlfxemM25f9ZJlXaUm/VBMLJ+x6D7E0i+ea8B8zf3VqVjAreOwCR2+0NvKJixvrY",
        "windows_amd64": "sha384-OrYnZnmhxfPbyVVsNfR06D0nlIP6wDeFPrgML0L9Epg73a32HA8c5LPYJn18g/II",
    },
    "4.30.4": {
        "darwin_amd64": "sha384-eYHqU7tN4p1fgHnCmsHmgWAzEjCh6zyTA5pCGfE1tH7XJQ2Dr5NwUFAyMTRmnqGp",
        "darwin_arm64": "sha384-iPAb+GPe4+S5j906JRfJgiB50tQJd+AlJiF6HuCOMEsqTsyNjAid+ag7Lt49HW2E",
        "linux_amd64": "sha384-KPG44IxvaQw0lg93R6tZbxNFDzw8vgCSuJc4vveDe6/cIVnQMZB4OPRGiCU1n+z2",
        "linux_arm64": "sha384-3EeHJQXwhXnwnmI48nH9BT5PRxvMQUw2yeI2Q8jRONSE3nVoZy3PDPhkDi4U9k/1",
        "linux_s390x": "sha384-tMgt6wKBVtjQKwNMC6hhHUz3VlgYomazAwqsGmiV/acqO5Nq+gHDqbm7WZeSx1Lu",
        "linux_ppc64le": "sha384-EiVxl0MmOD5ExaZx9rJA4qiBUf+M8DLkrHpEBniNjMn/XaUAbEp8lDfclkQ9o1oi",
        "windows_amd64": "sha384-F93l6mZThCLAudlFJXlzRBXg+YWO/1N8Va27QEoOCfcuJ7wJsVIPoAFcuP3VOtia",
    },
    "4.30.3": {
        "darwin_amd64": "sha384-aF1mKEe13/8ycOr6o3H7wpDB2mvLNdQvvW6Sv19AxRl8KWvl88+ELpo/nUpSZy3s",
        "darwin_arm64": "sha384-TO5/c3FC4xxxWEu3IU/s+g7vcOL0YPJwMLLQC85sF1OM4wOxzAinI0hxsxfynQdU",
        "linux_amd64": "sha384-PM/GG/kKT2iC5MD1hYh6R//F1lVhwBNFV3vnuB66K5x1NYqu6Tbei0wxxpkOAn1L",
        "linux_arm64": "sha384-2Ynn7Pw2dY6iIVBilF5OuNMscyQzeExbDhm6oVpCmXu4uhMW/3qS7KFOLE1rx/2s",
        "linux_s390x": "sha384-P4F2B8oWjlasazLRyKnG6HMJ+WAHwJG17bLBCmbGLtmL+J47IYve3Go8SDytNhbq",
        "linux_ppc64le": "sha384-oAlKG4DpmybovYjBAzxSx8dUuAjicKvC+DPdVHRhKxmyU6YsZtpA2qhksO0Dmka1",
        "windows_amd64": "sha384-+zAMM9aobwSio58t77oIQs1YuCQ5AM8UTUnyl+XHMIm9LHkKLTSPEgWur1HJuutx",
    },
    "4.30.2": {
        "darwin_amd64": "sha384-rXpT6g1AU269cdK9OM/E+AWd1ebDCP2/2B4YWr3ReacBhhagvHCTBDS1ooc6adi4",
        "darwin_arm64": "sha384-rn68m4/REEBNX37NjX4EAZw3aBtqZTk8oOlyTTd3Hjpa2zy/zdXtQq4Q52CRvh7r",
        "linux_amd64": "sha384-AH3aFTqsVST0hCcHj7FwFJ7wmY/n3q0fzi/ZqerAwxRmX80W6n/v9PZN4U5EyYXu",
        "linux_arm64": "sha384-tXAeZo6umhL1BRm4xArb97DW8UT34b1JP3OxUYQ3ugSb+Td7SKDTih3dCWXDozxH",
        "linux_s390x": "sha384-WMnziv/oMWXNNVdr5fCP7mSPg1UlFR8chnazOEnycwLsB2M7LxkENuQeYfqbstff",
        "linux_ppc64le": "sha384-S4FYx4L7BgrVJdU35uKoxm4/4kmweDP55GF3qmw7wcJzlNu552blknJDNJMMB2Gb",
        "windows_amd64": "sha384-VDJA/2vyvPmiQFstAIqp1WShmfnBtUysPQHupWptLZ6rmplSYBLYL6UtqOtHdMaI",
    },
    "4.30.1": {
        "darwin_amd64": "sha384-D2naPOWkB7iBbWOfNiM5RCXeHJvOLH/ib8wjE5f0SEvKhRSJ7eQ8fnVHzw1Zzi/w",
        "darwin_arm64": "sha384-JDQWrvpu3EoRVJT8qqmEyIvD2ovm48BrxOQgjJcTeY7/U8izuwPH7zrshnYxao0R",
        "linux_amd64": "sha384-k/N+JzgqLRjkDDuq6XPCJyS32B2zuM/k1feOI6/VqwFGZS0oXKGNnIdhDi1OmjVt",
        "linux_arm64": "sha384-CfiZSUynXXVlsbjVJbPPbQP2/BtxNSNem8KudyTiFaR5qHlldLo4Az9NaelRrmZv",
        "linux_s390x": "sha384-mMKIb35SdLlqJMjUflId7zBE+n24+5EQSk5qwDaSJ23sybvr36FIu8WvakopP52U",
        "linux_ppc64le": "sha384-FqwY/8VundrO775EF3VoMF2YeMEVjBACg4wE9YnjChj9nt2B95c2sjVQCbESEqtu",
        "windows_amd64": "sha384-OCdYU2tB5aPlqjqL0NyGBWe87v5rn0kjXvbJdz5V1DC+pK2y6aZwOxLVUqgt8byp",
    },
    "4.29.2": {
        "darwin_amd64": "sha384-N9MXB/2SOWuL07IG9ShVRWIUnr38y+Ly7hQIq42MsxPbNEGlt8JG5BzMmfxlFWVu",
        "darwin_arm64": "sha384-wwiVQYzZ6yaZ3MpwEffha1JyDEPo8BL7chaSE1DnupldLQES3k8rXSldynQteysD",
        "linux_amd64": "sha384-zViR/M36ICnNzFUN0vWKCHOna86inL0aFYrETjd6PSx8Eqbl13jNcZ7hHv6AeKSk",
        "linux_arm64": "sha384-pChFNLv6hf1Y3r4QMu8D9NqFcUuQZdGo9WpLW3mUUU4/0nGu6mKRb+cGx1K9QLYg",
        "linux_s390x": "sha384-Qm/7ICuxR3kB3Uovt3P1aZMlHNUVSvLpIwfudaz0Vdkx/wxvJuHqCqcjWZ0DRCev",
        "linux_ppc64le": "sha384-oI/Of8ncfLFQKgA+P/bfDD8O+KrOTy4qatrd4KoO1172XyJz14DHIeWoFTXOBl2m",
        "windows_amd64": "sha384-Zg0Ltumeuy+8czlhBXpdHeKNd8rflWJGMBCrO1AwAY6KsFUSuSGeXDi3wMotT+Qv",
    },
    "4.29.1": {
        "darwin_amd64": "sha384-W3nmcSOs6i42/1JljCDefTje22Q6t8zMryNVK9JbhtIghZ9VlbeE9E8az5su0BNQ",
        "darwin_arm64": "sha384-1xPziJ5N275Y46ZlYDfz7IocliIR+MCpPG9KrXfo7IA/KW5qV9aM7EhDjjf/aiXs",
        "linux_amd64": "sha384-waB6IJiMLTRG7At3RmL4MdpRZaaWNTDy6GOy1TpQK24NbQsnTD8QHGtF4mX2un71",
        "linux_arm64": "sha384-nfXpDxuyuoKjNtEr4Gw0ShuAqWsM08q4pceDwhWFg4vKJe9Z7A26iqeGxPI9daF+",
        "linux_s390x": "sha384-X4lOVFgaJs2ESaLCASM6vqHgQyasmMlFFEKqQtD5qBs3txEkLz/xUyJ96BDdFF4B",
        "linux_ppc64le": "sha384-zXDgMcUpMVdVgefD+tqNHpwcr5CaxsQ9iKuPcWXeeNWQ5WHM+s/imWv+jCSWLVbq",
        "windows_amd64": "sha384-x6d2r2cAPtsCeJ3b9gJeas4hOWef3iF7pYmpr3jMuCmNgTH0NYQR9ghdu7Bxr1bB",
    },
    "4.28.2": {
        "darwin_amd64": "sha384-4AXeU2VHBbcMuj41t/uQ1GDZgOCWpautbnoOoBkxKlpF70BPsJQcfJADHbfkKpWL",
        "darwin_arm64": "sha384-GjK93wtKupFpR1JBmVsoxpoM6KsNZ4DprT6kZ2j1xTYWjYbpUXfF65pUMiAbhLU1",
        "linux_amd64": "sha384-kp3GzKtmoz1XgkCjztSu5NsqsWK6BiumFQuPVP0iVcKnSoRR78q+51COpFmKnOze",
        "linux_arm64": "sha384-a6EOkjRqT7AYAT7Kx8x9VERiFNax+4mH3FGrsLf+tNKxN1Uglfcp5qHsD1wLRTAQ",
        "linux_s390x": "sha384-RnJdSQo6+JK73PIWNFVbSey05xxfVQ2T5iahFE/zzd4wwwaWCVq560UskTnj1hX3",
        "linux_ppc64le": "sha384-Qnhh8beQEBVBGmq3g19UqGRklFA/w9fApAlp65qn7fjClZaU9feg8wtXbFuE34AJ",
        "windows_amd64": "sha384-e+LWtKw1AC69Ma2pWD84CGjvX38y8zWVKlOzm+Pf2HHYjtMae9JYxN6EsvHDdlBE",
    },
    "4.28.1": {
        "darwin_amd64": "sha384-8lV1gSf41Ud4XpuaQ+0TE5OEmBOfgJVLcoa3SjxsTE56RL0MgVoqa8A6CrOdzYTC",
        "darwin_arm64": "sha384-t0noxlOYiv2xyjfBskOMTcsCz+AwfXaPRhgD//yeGgRAIiylTgp7LFy3HKrSqjtN",
        "linux_amd64": "sha384-n3Y2PYn9/Ydwti+pqqlDOxpM19E/InXo95kDiywb2NEjqHfzzEN57MRdm0kdd8bK",
        "linux_arm64": "sha384-W+fkBuf5vrofHYXI0qBUefP63TUYnhwGAAi09YxgIBFk09BUG8Fk1coPzCOaEJ7d",
        "linux_s390x": "sha384-1+D4UGDjssGnCGK8S1X69kMla5J7/1cu+Y0lD2yAIEGneLz2NtvxiN3mtalwTWaN",
        "linux_ppc64le": "sha384-jXtlobgI4JAXQUSvQER3oWuzSWil4rm13eyxVOorzzKtvaG1eAXl+skOH+8BNEqL",
        "windows_amd64": "sha384-DooYFtPjxiuZ1EV/CStC4bklQgLBgxdW7r5jJ/+GgBkXltjkaZgI11XkHFhUAnxj",
    },
    "4.27.5": {
        "darwin_amd64": "sha384-vy5Lym+7yVMR6BQEBMvN0lM/b4h8t5B2RtbMO8suBuQov2Rshw5AtWKVm3FhWqbc",
        "darwin_arm64": "sha384-PQjvVAdRlJNdI7UmZzsZsan9E+pBAgrf6xKg5NmNUE4jXwT6VoM9ena82xL9Kc/H",
        "linux_amd64": "sha384-3FsXfJdVH9gK1FEBFZukRrqDZRUvz7YZxv4B33j9j7hnnI/tw9kF7rOFHIpKsDWd",
        "linux_arm64": "sha384-14bPrTE0pKfQD8q1TZ8uCy7Bt2fFJSMuaecJRyhjd0LCLBPw+UbuC7ZSsZknqgLG",
        "linux_s390x": "sha384-WQJOeiy+YRGBQLNEFfMy/xFI5OXHuKQOLJnAAyuRCvg4v427EvQbxMUAW+hr+uCL",
        "linux_ppc64le": "sha384-YhgIsXOPq2FpSiJGS6Q/yY9T5uki1vM+AEzJ2SnZuVGd9uX5iWAZN3ZHb5fv4rv0",
        "windows_amd64": "sha384-2oLJz7lym2uIm1QSgMBy3u0TsqZdQ6PFlSMUiLbn+e8jjaEAMHtnfqm338JDk92m",
    },
    "4.27.3": {
        "darwin_amd64": "sha384-fWL3t5KJZ4vJK9vjYI2Rk9QT+tW7zon4T+VE1XvmdwLn0oQH3R7BMc04jv90Qu8H",
        "darwin_arm64": "sha384-K/BhuDtIkBtfzdzMt0XvqQSwqkp8ynDnFJsMnmq+EXIWLHCXK1KD/yTowQOXGDeR",
        "linux_amd64": "sha384-1PwDnUZWikPr5eYn1vdQVJX523+Ftz5T5/YmaJhfWg1u+YkapFGzrFsVDh5JYXUv",
        "linux_arm64": "sha384-goxKzNBAuAt7F0Egqrg5JkdZPLMCOp7yaU7hGzrHWla5E4w4j/+2HVM3tsJaZK1R",
        "linux_s390x": "sha384-1MUbqdM9JkSzBWSVxhWPJwrYE6k0jtgWh8PnLYyugiVbE2bOa2cuXdoGxfpgDIGH",
        "linux_ppc64le": "sha384-uHdGKiWTTpxnewNuR72XyZTeh0H8TSKU1q9EDAeuPEhCZzRYyP8W7XPS1oPaQ8l2",
        "windows_amd64": "sha384-fj/h6eZ/hWNl+ERH3FTPk3wVVxIf/9zecx42wtxuoSGpQfKWL7CGNbDRvWNAVyYv",
    },
    "4.27.2": {
        "darwin_amd64": "sha384-dDG2xx6W+LWP922Ihe2dhacnzhjXJBklZSyJCG5anFGYZn7N2xqWpTookLfegVC2",
        "darwin_arm64": "sha384-bfHab2ci86GRlE3jAbKLPvAj1ulnr0N+GNC/1frcYaGFOKjrFN7lia/KPPU/ZFVJ",
        "linux_amd64": "sha384-sl0i3SVGdOQCc/vf09VwDPhiPM9W/wG2yMhjM51oyVVDHrLs63kHMnTqaCvr/e6n",
        "linux_arm64": "sha384-bJqQ2BcX20CVmaH9c25rdIr9WNqtdTHWXzACQTDvPHnSnHrTpEJrWP57sVGGwyZ+",
        "linux_s390x": "sha384-mLaO2DJGgqBn79sASogwX1qCvBALNWjvGfV2fDuuLuKzFqQmTZKXghLiv8rbZ2G+",
        "linux_ppc64le": "sha384-DwlV76JUvOV6J8MD9g48rQfzFCPC9hi5eRhz1iVrNRfKoGqfGkHxufpmAWYpRWVH",
        "windows_amd64": "sha384-T0Vc4hlJnh8eoa04I3Q4/BIbGOch31OxTUsHDkzlku/s5r2Ms9Y+YopYksmK9FcH",
    },
    "4.26.1": {
        "darwin_amd64": "sha384-CXHTzHhkXvpAc2hCn7VizSzbfJDM0DO6LI4Gy9iOx3kODMi6v21AdEhSPiAsG36z",
        "darwin_arm64": "sha384-BwsMtgoteA8n2xP/64E9gDF9jz7mTpAz4z90NqbnoJSau+ri/Mh8Bw7Ymb9A31GN",
        "linux_amd64": "sha384-/cy001wSx8tbEmhibNe9ef3cFY1oAjm5Ae3+OBEezI45unE/BYnyzaodk2tS+s5i",
        "linux_arm64": "sha384-kHYvn9xiQlwvfPy7CBuc7bbXK6Z+cliKIYmk9rQs0hQIXps9HpjTCH1XNXUCm9WX",
        "linux_s390x": "sha384-WuiL9i5iMHZ8gQYmTjoiX2LK6WC2bLFYhhh7to1/qjn0U6ZvI3ylhORkjZ84WnN2",
        "linux_ppc64le": "sha384-LyU+MR2uLLEYpRZJriKCcGSR3iXZ+C2erLmuydM4YsXrucPmUkigfG8iM3uzF0I/",
        "windows_amd64": "sha384-shjvmGSa79aYKqeWLlOuzrHUvnrK75Y0ku4obQ/k+s1vofZIkevdW8UnW1SNrOMe",
    },
    "4.25.3": {
        "darwin_amd64": "sha384-8Ok6gt8rA/4iPTx3yGs4ZrJWWmKwWKQuYbuVpYHuzQJE5eHGz5ePh1dPvmnm81Y5",
        "darwin_arm64": "sha384-gIEE8Jymptr9vftXY8hvFwUsWJ3drmAWGxMjsv0YeIGF7C4LksvYUufKDRgvPKis",
        "linux_amd64": "sha384-ziQP4MAN3jJkT8XTL05ZI4SxadBN9VyQqVcR1+53uDANaCto7t0p3uWL7IcqZ3to",
        "linux_arm64": "sha384-FLKcZBzUnzRWOVOTFv1mm59aD7K0d9hRthO5Eatonz/oHXSJw8BjTzvHasRkqjlZ",
        "linux_s390x": "sha384-K39zTD8zJnI8kR4NfSvnrLd3oEoNuBCIgCmC+2nwzEpn+SqVvNFESIdh+ui/7RcX",
        "linux_ppc64le": "sha384-iTzvYkCHIWc9keN0tM9pOPhGyzxdwdROWQYmtragW0F1aPWTFR4+YxX9rhbwO2W/",
        "windows_amd64": "sha384-KwQmB+VjNuPJZkMJn7OnFa/WwXdLxjrno6eeMSA1sxZeBjrXSPzIadwY271n/fNn",
    },
    "4.25.2": {
        "darwin_amd64": "sha384-JOSKdcN5JoSfryJBDFzTSk21uFVNw+bWdLHTErwIyaUOAZZV9LISzlmsYiQoPfo3",
        "darwin_arm64": "sha384-COVD0Ko3vQUmSS/SS17VrpA4gTaxuyszgPfE9P8vNKLlB+65I7HznstHg/iElxrk",
        "linux_amd64": "sha384-uK7+qPUYBO1bnKUNYKqKuGpfTKsRktfptGATlV14AFb5hiVp6vcWvDyTRSfiZn5f",
        "linux_arm64": "sha384-2SwY2OtotYhupSEeUSHRKTMbUoIhUyIt7QgujH3UyaQ/9AoXWNZ3h05OAdWL5WbL",
        "linux_s390x": "sha384-AXZe1USYMi0mg5ylHnU8OGqQQZcMDETD/bLWG0BKytSBhZ0Xkkhn9r90fJYM4xVS",
        "linux_ppc64le": "sha384-N6UXrIzrpPy6zojzI8J7AFP5z3UAe9KU8WLuUKe8WrttuD1Fl3YTwvo/OM2+5NTt",
        "windows_amd64": "sha384-wD0GpKHTLQIWWNJv43ts+l2rIdAiyYYtb3upncdIOLLydFcWxB39+thcQ8aSdaW+",
    },
    "4.24.5": {
        "darwin_amd64": "sha384-Y6Utm9NAX7q69apRHLAU6oNYk5Kn5b6LUccBolbTm2CXXYye8pabeFPsaREFIHbw",
        "darwin_arm64": "sha384-d6+hFiZrsUeqnXJufnvadTi0BL/sfbd6K7LnJyLVDy31C0isjyHipVqlibKYbFSu",
        "linux_amd64": "sha384-FEWzb66XTTiMfz5wA/hCs/n0N+PVj4lXzKX8ZIUXnM3JTlFlBvA9X59elqqEJUPq",
        "linux_arm64": "sha384-u8H3RxTssXKr1lEylydi1tzXKKsoax7aDXi4R/JF8irZ7RTwCqU/ogMj30B0Xo01",
        "linux_s390x": "sha384-ccipOj8IBVDb6ZxBYDyRDVvfOTHRSD4nGuMbikrDrigGdYyI/iVb+R8lb6kdLarb",
        "linux_ppc64le": "sha384-HWzKwuNx+uZI/8KXSNFVg+drCZiZU/17hIl8gG+b+UyLMAFZ/sOB/nu7yzEOdzvH",
        "windows_amd64": "sha384-6T42wIkqXZ8OCetIeMjTlTIVQDwlRpTXj8pi+SrGzU4r5waq3SwIYSrDqUxMD43j",
    },
    "4.24.4": {
        "darwin_amd64": "sha384-H5JnUD7c0jpbOvvN1pGz12XFi3XrX+ism4iGnH9wv37i+qdkD2AdTbTe4MIFtMR+",
        "darwin_arm64": "sha384-9B85+dFTGRmMWWP2M+PVOkl8CtAb/HV4+XNGC0OBfdBvdJU85FyiTb12XGEgNjFp",
        "linux_amd64": "sha384-y8vr5fWIqSvJhMoHwldoVPOJpAfLi4iHcnhfTcm/nuJAxGAJmI2MiBbk3t7lQNHC",
        "linux_arm64": "sha384-nxvFzxOVNtbt1lQZshkUnM6SHQnXKkzWKEw4TzU9HOms6mUJnYbYXc0x0LwPkpQK",
        "linux_s390x": "sha384-525bIc8L80mIMVH+PmNDi4vBP4AfvBw/736ISW0F7+7zowSYOUK+EN/REo31kNdN",
        "linux_ppc64le": "sha384-Sm3PniOqhRIlYaVBZOwncKRpPDLhiuHNCvVWUW9ihnAQM3woXvhb5iNfbws0Rz+G",
        "windows_amd64": "sha384-f8jkaz3oRaDcn8jiXupeDO665t6d2tTnFuU0bKwLWszXSz8r29My/USG+UoO9hOr",
    },
}

YqInfo = provider(
    doc = "Provide info for executing yq",
    fields = {
        "bin": "Executable yq binary",
    },
)

def _yq_toolchain_impl(ctx):
    binary = ctx.file.bin

    # Make the $(YQ_BIN) variable available in places like genrules.
    # See https://docs.bazel.build/versions/main/be/make-variables.html#custom_variables
    template_variables = platform_common.TemplateVariableInfo({
        "YQ_BIN": binary.path,
    })
    default_info = DefaultInfo(
        files = depset([binary]),
        runfiles = ctx.runfiles(files = [binary]),
    )
    yq_info = YqInfo(
        bin = binary,
    )

    # Export all the providers inside our ToolchainInfo
    # so the resolved_toolchain rule can grab and re-export them.
    toolchain_info = platform_common.ToolchainInfo(
        yqinfo = yq_info,
        template_variables = template_variables,
        default = default_info,
    )

    return [default_info, toolchain_info, template_variables]

yq_toolchain = rule(
    implementation = _yq_toolchain_impl,
    attrs = {
        "bin": attr.label(
            mandatory = True,
            allow_single_file = True,
        ),
    },
)

def _yq_toolchains_repo_impl(rctx):
    # Expose a concrete toolchain which is the result of Bazel resolving the toolchain
    # for the execution or target platform.
    # Workaround for https://github.com/bazelbuild/bazel/issues/14009
    starlark_content = """# @generated by @aspect_bazel_lib//lib/private:yq_toolchain.bzl

# Forward all the providers
def _resolved_toolchain_impl(ctx):
    toolchain_info = ctx.toolchains["@aspect_bazel_lib//lib:yq_toolchain_type"]
    return [
        toolchain_info,
        toolchain_info.default,
        toolchain_info.yqinfo,
        toolchain_info.template_variables,
    ]

# Copied from java_toolchain_alias
# https://cs.opensource.google/bazel/bazel/+/master:tools/jdk/java_toolchain_alias.bzl
resolved_toolchain = rule(
    implementation = _resolved_toolchain_impl,
    toolchains = ["@aspect_bazel_lib//lib:yq_toolchain_type"],
    incompatible_use_toolchain_transition = True,
)
"""
    rctx.file("defs.bzl", starlark_content)

    build_content = """# @generated by @aspect_bazel_lib//lib/private:yq_toolchain.bzl
#
# These can be registered in the workspace file or passed to --extra_toolchains flag.
# By default all these toolchains are registered by the yq_register_toolchains macro
# so you don't normally need to interact with these targets.

load(":defs.bzl", "resolved_toolchain")

resolved_toolchain(name = "resolved_toolchain", visibility = ["//visibility:public"])

"""

    for [platform, meta] in YQ_PLATFORMS.items():
        build_content += """
toolchain(
    name = "{platform}_toolchain",
    exec_compatible_with = {compatible_with},
    toolchain = "@{user_repository_name}_{platform}//:yq_toolchain",
    toolchain_type = "@aspect_bazel_lib//lib:yq_toolchain_type",
)
""".format(
            platform = platform,
            user_repository_name = rctx.attr.user_repository_name,
            compatible_with = meta.compatible_with,
        )

    # Base BUILD file for this repository
    rctx.file("BUILD.bazel", build_content)

yq_toolchains_repo = repository_rule(
    _yq_toolchains_repo_impl,
    doc = """Creates a repository with toolchain definitions for all known platforms
     which can be registered or selected.""",
    attrs = {
        "user_repository_name": attr.string(doc = "Base name for toolchains repository"),
    },
)

def _yq_platform_repo_impl(rctx):
    is_windows = rctx.attr.platform.startswith("windows_")
    meta = YQ_PLATFORMS[rctx.attr.platform]
    release_platform = meta.release_platform if hasattr(meta, "release_platform") else rctx.attr.platform

    #https://github.com/mikefarah/yq/releases/download/v4.24.4/yq_linux_386
    url = "https://github.com/mikefarah/yq/releases/download/v{0}/yq_{1}{2}".format(
        rctx.attr.version,
        release_platform,
        ".exe" if is_windows else "",
    )

    rctx.download(
        url = url,
        output = "yq.exe" if is_windows else "yq",
        executable = True,
        integrity = YQ_VERSIONS[rctx.attr.version][release_platform],
    )
    build_content = """# @generated by @aspect_bazel_lib//lib/private:yq_toolchain.bzl
load("@aspect_bazel_lib//lib/private:yq_toolchain.bzl", "yq_toolchain")
exports_files(["{0}"])
yq_toolchain(name = "yq_toolchain", bin = "{0}", visibility = ["//visibility:public"])
""".format("yq.exe" if is_windows else "yq")

    # Base BUILD file for this repository
    rctx.file("BUILD.bazel", build_content)

yq_platform_repo = repository_rule(
    implementation = _yq_platform_repo_impl,
    doc = "Fetch external tools needed for yq toolchain",
    attrs = {
        "version": attr.string(mandatory = True, values = YQ_VERSIONS.keys()),
        "platform": attr.string(mandatory = True, values = YQ_PLATFORMS.keys()),
    },
)

def _yq_host_alias_repo(rctx):
    ext = ".exe" if repo_utils.is_windows(rctx) else ""

    # Base BUILD file for this repository
    rctx.file("BUILD.bazel", """# @generated by @aspect_bazel_lib//lib/private:yq_toolchain.bzl
package(default_visibility = ["//visibility:public"])
exports_files(["yq{ext}"])
""".format(
        ext = ext,
    ))

    rctx.symlink("../{name}_{platform}/yq{ext}".format(
        name = rctx.attr.name,
        platform = repo_utils.platform(rctx),
        ext = ext,
    ), "yq{ext}".format(ext = ext))

yq_host_alias_repo = repository_rule(
    _yq_host_alias_repo,
    doc = """Creates a repository with a shorter name meant for the host platform, which contains
    a BUILD.bazel file that exports symlinks to the host platform's binaries
    """,
)
