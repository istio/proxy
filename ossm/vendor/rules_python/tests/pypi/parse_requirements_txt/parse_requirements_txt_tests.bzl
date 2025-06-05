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

"Unit tests for yaml.bzl"

load("@bazel_skylib//lib:unittest.bzl", "asserts", "unittest")
load("//python/private/pypi:parse_requirements_txt.bzl", "parse_requirements_txt")  # buildifier: disable=bzl-visibility

def _parse_basic_test_impl(ctx):
    env = unittest.begin(ctx)

    # Base cases
    asserts.equals(env, [], parse_requirements_txt("").requirements)
    asserts.equals(env, [], parse_requirements_txt("\n").requirements)

    # Various requirement specifiers (https://pip.pypa.io/en/stable/reference/requirement-specifiers/#requirement-specifiers)
    asserts.equals(env, [("SomeProject", "SomeProject")], parse_requirements_txt("SomeProject\n").requirements)
    asserts.equals(env, [("SomeProject", "SomeProject == 1.3")], parse_requirements_txt("SomeProject == 1.3\n").requirements)
    asserts.equals(env, [("SomeProject", "SomeProject >= 1.2, < 2.0")], parse_requirements_txt("SomeProject >= 1.2, < 2.0\n").requirements)
    asserts.equals(env, [("SomeProject", "SomeProject[foo, bar]")], parse_requirements_txt("SomeProject[foo, bar]\n").requirements)
    asserts.equals(env, [("SomeProject", "SomeProject ~= 1.4.2")], parse_requirements_txt("SomeProject ~= 1.4.2\n").requirements)
    asserts.equals(env, [("SomeProject", "SomeProject == 5.4 ; python_version < '3.8'")], parse_requirements_txt("SomeProject == 5.4 ; python_version < '3.8'\n").requirements)
    asserts.equals(env, [("SomeProject", "SomeProject ; sys_platform == 'win32'")], parse_requirements_txt("SomeProject ; sys_platform == 'win32'\n").requirements)
    asserts.equals(env, [("requests", "requests [security] >= 2.8.1, == 2.8.* ; python_version < 2.7")], parse_requirements_txt("requests [security] >= 2.8.1, == 2.8.* ; python_version < 2.7\n").requirements)

    # Multiple requirements
    asserts.equals(env, [("FooProject", "FooProject==1.0.0"), ("BarProject", "BarProject==2.0.0")], parse_requirements_txt("""\
FooProject==1.0.0
BarProject==2.0.0
""").requirements)

    asserts.equals(env, [("FooProject", "FooProject==1.0.0"), ("BarProject", "BarProject==2.0.0")], parse_requirements_txt("""\
FooProject==1.0.0

BarProject==2.0.0
""").requirements)

    # Comments
    asserts.equals(env, [("SomeProject", "SomeProject")], parse_requirements_txt("""\
# This is a comment
SomeProject
""").requirements)
    asserts.equals(env, [("SomeProject", "SomeProject")], parse_requirements_txt("""\
SomeProject
# This is a comment
""").requirements)
    asserts.equals(env, [("SomeProject", "SomeProject == 1.3")], parse_requirements_txt("""\
SomeProject == 1.3 # This is a comment
""").requirements)
    asserts.equals(env, [("FooProject", "FooProject==1.0.0"), ("BarProject", "BarProject==2.0.0")], parse_requirements_txt("""\
FooProject==1.0.0
# Comment
BarProject==2.0.0 #Comment
""").requirements)
    asserts.equals(env, [("requests", "requests @ https://github.com/psf/requests/releases/download/v2.29.0/requests-2.29.0.tar.gz#sha1=3897c249b51a1a405d615a8c9cb92e5fdbf0dd49")], parse_requirements_txt("""\
requests @ https://github.com/psf/requests/releases/download/v2.29.0/requests-2.29.0.tar.gz#sha1=3897c249b51a1a405d615a8c9cb92e5fdbf0dd49
""").requirements)

    # Multiline
    asserts.equals(env, [("certifi", "certifi==2021.10.8     --hash=sha256:78884e7c1d4b00ce3cea67b44566851c4343c120abd683433ce934a68ea58872     --hash=sha256:d62a0163eb4c2344ac042ab2bdf75399a71a2d8c7d47eac2e2ee91b9d6339569")], parse_requirements_txt("""\
certifi==2021.10.8 \
    --hash=sha256:78884e7c1d4b00ce3cea67b44566851c4343c120abd683433ce934a68ea58872 \
    --hash=sha256:d62a0163eb4c2344ac042ab2bdf75399a71a2d8c7d47eac2e2ee91b9d6339569
    # via requests
""").requirements)
    asserts.equals(env, [("requests", "requests @ https://github.com/psf/requests/releases/download/v2.29.0/requests-2.29.0.tar.gz#sha1=3897c249b51a1a405d615a8c9cb92e5fdbf0dd49     --hash=sha256:eca58eb564b134e4ff521a02aa6f566c653835753e1fc8a50a20cb6bee4673cd")], parse_requirements_txt("""\
requests @ https://github.com/psf/requests/releases/download/v2.29.0/requests-2.29.0.tar.gz#sha1=3897c249b51a1a405d615a8c9cb92e5fdbf0dd49 \
    --hash=sha256:eca58eb564b134e4ff521a02aa6f566c653835753e1fc8a50a20cb6bee4673cd
    # via requirements.txt
""").requirements)

    # Options
    asserts.equals(env, ["--pre"], parse_requirements_txt("--pre\n").options)
    asserts.equals(env, ["--find-links", "/my/local/archives"], parse_requirements_txt("--find-links /my/local/archives\n").options)
    asserts.equals(env, ["--pre", "--find-links", "/my/local/archives"], parse_requirements_txt("""\
--pre
--find-links /my/local/archives
""").options)
    asserts.equals(env, ["--pre", "--find-links", "/my/local/archives"], parse_requirements_txt("""\
--pre # Comment
--find-links /my/local/archives
""").options)
    asserts.equals(env, struct(requirements = [("FooProject", "FooProject==1.0.0")], options = ["--pre", "--find-links", "/my/local/archives"]), parse_requirements_txt("""\
--pre # Comment
FooProject==1.0.0
--find-links /my/local/archives
"""))

    return unittest.end(env)

def _parse_requirements_lockfile_test_impl(ctx):
    env = unittest.begin(ctx)

    asserts.equals(env, [
        ("certifi", "certifi==2021.10.8     --hash=sha256:78884e7c1d4b00ce3cea67b44566851c4343c120abd683433ce934a68ea58872     --hash=sha256:d62a0163eb4c2344ac042ab2bdf75399a71a2d8c7d47eac2e2ee91b9d6339569"),
        ("chardet", "chardet==4.0.0     --hash=sha256:0d6f53a15db4120f2b08c94f11e7d93d2c911ee118b6b30a04ec3ee8310179fa     --hash=sha256:f864054d66fd9118f2e67044ac8981a54775ec5b67aed0441892edb553d21da5"),
        ("idna", "idna==2.10     --hash=sha256:b307872f855b18632ce0c21c5e45be78c0ea7ae4c15c828c20788b26921eb3f6     --hash=sha256:b97d804b1e9b523befed77c48dacec60e6dcb0b5391d57af6a65a312a90648c0"),
        ("pathspec", "pathspec==0.9.0     --hash=sha256:7d15c4ddb0b5c802d161efc417ec1a2558ea2653c2e8ad9c19098201dc1c993a     --hash=sha256:e564499435a2673d586f6b2130bb5b95f04a3ba06f81b8f895b651a3c76aabb1"),
        ("python-dateutil", "python-dateutil==2.8.2     --hash=sha256:0123cacc1627ae19ddf3c27a5de5bd67ee4586fbdd6440d9748f8abb483d3e86     --hash=sha256:961d03dc3453ebbc59dbdea9e4e11c5651520a876d0f4db161e8674aae935da9"),
        ("python-magic", "python-magic==0.4.24     --hash=sha256:4fec8ee805fea30c07afccd1592c0f17977089895bdfaae5fec870a84e997626     --hash=sha256:de800df9fb50f8ec5974761054a708af6e4246b03b4bdaee993f948947b0ebcf"),
        ("pyyaml", "pyyaml==6.0     --hash=sha256:0283c35a6a9fbf047493e3a0ce8d79ef5030852c51e9d911a27badfde0605293     --hash=sha256:055d937d65826939cb044fc8c9b08889e8c743fdc6a32b33e2390f66013e449b     --hash=sha256:07751360502caac1c067a8132d150cf3d61339af5691fe9e87803040dbc5db57     --hash=sha256:0b4624f379dab24d3725ffde76559cff63d9ec94e1736b556dacdfebe5ab6d4b     --hash=sha256:0ce82d761c532fe4ec3f87fc45688bdd3a4c1dc5e0b4a19814b9009a29baefd4     --hash=sha256:1e4747bc279b4f613a09eb64bba2ba602d8a6664c6ce6396a4d0cd413a50ce07     --hash=sha256:213c60cd50106436cc818accf5baa1aba61c0189ff610f64f4a3e8c6726218ba     --hash=sha256:231710d57adfd809ef5d34183b8ed1eeae3f76459c18fb4a0b373ad56bedcdd9     --hash=sha256:277a0ef2981ca40581a47093e9e2d13b3f1fbbeffae064c1d21bfceba2030287     --hash=sha256:2cd5df3de48857ed0544b34e2d40e9fac445930039f3cfe4bcc592a1f836d513     --hash=sha256:40527857252b61eacd1d9af500c3337ba8deb8fc298940291486c465c8b46ec0     --hash=sha256:473f9edb243cb1935ab5a084eb238d842fb8f404ed2193a915d1784b5a6b5fc0     --hash=sha256:48c346915c114f5fdb3ead70312bd042a953a8ce5c7106d5bfb1a5254e47da92     --hash=sha256:50602afada6d6cbfad699b0c7bb50d5ccffa7e46a3d738092afddc1f9758427f     --hash=sha256:68fb519c14306fec9720a2a5b45bc9f0c8d1b9c72adf45c37baedfcd949c35a2     --hash=sha256:77f396e6ef4c73fdc33a9157446466f1cff553d979bd00ecb64385760c6babdc     --hash=sha256:819b3830a1543db06c4d4b865e70ded25be52a2e0631ccd2f6a47a2822f2fd7c     --hash=sha256:897b80890765f037df3403d22bab41627ca8811ae55e9a722fd0392850ec4d86     --hash=sha256:98c4d36e99714e55cfbaaee6dd5badbc9a1ec339ebfc3b1f52e293aee6bb71a4     --hash=sha256:9df7ed3b3d2e0ecfe09e14741b857df43adb5a3ddadc919a2d94fbdf78fea53c     --hash=sha256:9fa600030013c4de8165339db93d182b9431076eb98eb40ee068700c9c813e34     --hash=sha256:a80a78046a72361de73f8f395f1f1e49f956c6be882eed58505a15f3e430962b     --hash=sha256:b3d267842bf12586ba6c734f89d1f5b871df0273157918b0ccefa29deb05c21c     --hash=sha256:b5b9eccad747aabaaffbc6064800670f0c297e52c12754eb1d976c57e4f74dcb     --hash=sha256:c5687b8d43cf58545ade1fe3e055f70eac7a5a1a0bf42824308d868289a95737     --hash=sha256:cba8c411ef271aa037d7357a2bc8f9ee8b58b9965831d9e51baf703280dc73d3     --hash=sha256:d15a181d1ecd0d4270dc32edb46f7cb7733c7c508857278d3d378d14d606db2d     --hash=sha256:d4db7c7aef085872ef65a8fd7d6d09a14ae91f691dec3e87ee5ee0539d516f53     --hash=sha256:d4eccecf9adf6fbcc6861a38015c2a64f38b9d94838ac1810a9023a0609e1b78     --hash=sha256:d67d839ede4ed1b28a4e8909735fc992a923cdb84e618544973d7dfc71540803     --hash=sha256:daf496c58a8c52083df09b80c860005194014c3698698d1a57cbcfa182142a3a     --hash=sha256:e61ceaab6f49fb8bdfaa0f92c4b57bcfbea54c09277b1b4f7ac376bfb7a7c174     --hash=sha256:f84fbc98b019fef2ee9a1cb3ce93e3187a6df0b2538a651bfb890254ba9f90b5"),
        ("requests", "requests==2.25.1     --hash=sha256:27973dd4a904a4f13b263a19c866c13b92a39ed1c964655f025f3f8d3d75b804     --hash=sha256:c210084e36a42ae6b9219e00e48287def368a26d03a048ddad7bfee44f75871e"),
        ("s3cmd", "s3cmd==2.1.0     --hash=sha256:49cd23d516b17974b22b611a95ce4d93fe326feaa07320bd1d234fed68cbccfa     --hash=sha256:966b0a494a916fc3b4324de38f089c86c70ee90e8e1cae6d59102103a4c0cc03"),
        ("six", "six==1.16.0     --hash=sha256:1e61c37477a1626458e36f7b1d82aa5c9b094fa4802892072e49de9c60c4c926     --hash=sha256:8abb2f1d86890a2dfb989f9a77cfcfd3e47c2a354b01111771326f8aa26e0254"),
        ("urllib3", "urllib3==1.26.7     --hash=sha256:4987c65554f7a2dbf30c18fd48778ef124af6fab771a377103da0585e2336ece     --hash=sha256:c4fdf4019605b6e5423637e01bc9fe4daef873709a7973e195ceba0a62bbc844"),
        ("yamllint", "yamllint==1.26.3     --hash=sha256:3934dcde484374596d6b52d8db412929a169f6d9e52e20f9ade5bf3523d9b96e"),
        ("setuptools", "setuptools==59.6.0     --hash=sha256:22c7348c6d2976a52632c67f7ab0cdf40147db7789f9aed18734643fe9cf3373     --hash=sha256:4ce92f1e1f8f01233ee9952c04f6b81d1e02939d6e1b488428154974a4d0783e"),
    ], parse_requirements_txt("""\
#
# This file is autogenerated by pip-compile with python 3.9
# To update, run:
#
#    bazel run //:requirements.update
#
certifi==2021.10.8 \
    --hash=sha256:78884e7c1d4b00ce3cea67b44566851c4343c120abd683433ce934a68ea58872 \
    --hash=sha256:d62a0163eb4c2344ac042ab2bdf75399a71a2d8c7d47eac2e2ee91b9d6339569
    # via requests
chardet==4.0.0 \
    --hash=sha256:0d6f53a15db4120f2b08c94f11e7d93d2c911ee118b6b30a04ec3ee8310179fa \
    --hash=sha256:f864054d66fd9118f2e67044ac8981a54775ec5b67aed0441892edb553d21da5
    # via requests
idna==2.10 \
    --hash=sha256:b307872f855b18632ce0c21c5e45be78c0ea7ae4c15c828c20788b26921eb3f6 \
    --hash=sha256:b97d804b1e9b523befed77c48dacec60e6dcb0b5391d57af6a65a312a90648c0
    # via requests
pathspec==0.9.0 \
    --hash=sha256:7d15c4ddb0b5c802d161efc417ec1a2558ea2653c2e8ad9c19098201dc1c993a \
    --hash=sha256:e564499435a2673d586f6b2130bb5b95f04a3ba06f81b8f895b651a3c76aabb1
    # via yamllint
python-dateutil==2.8.2 \
    --hash=sha256:0123cacc1627ae19ddf3c27a5de5bd67ee4586fbdd6440d9748f8abb483d3e86 \
    --hash=sha256:961d03dc3453ebbc59dbdea9e4e11c5651520a876d0f4db161e8674aae935da9
    # via s3cmd
python-magic==0.4.24 \
    --hash=sha256:4fec8ee805fea30c07afccd1592c0f17977089895bdfaae5fec870a84e997626 \
    --hash=sha256:de800df9fb50f8ec5974761054a708af6e4246b03b4bdaee993f948947b0ebcf
    # via s3cmd
pyyaml==6.0 \
    --hash=sha256:0283c35a6a9fbf047493e3a0ce8d79ef5030852c51e9d911a27badfde0605293 \
    --hash=sha256:055d937d65826939cb044fc8c9b08889e8c743fdc6a32b33e2390f66013e449b \
    --hash=sha256:07751360502caac1c067a8132d150cf3d61339af5691fe9e87803040dbc5db57 \
    --hash=sha256:0b4624f379dab24d3725ffde76559cff63d9ec94e1736b556dacdfebe5ab6d4b \
    --hash=sha256:0ce82d761c532fe4ec3f87fc45688bdd3a4c1dc5e0b4a19814b9009a29baefd4 \
    --hash=sha256:1e4747bc279b4f613a09eb64bba2ba602d8a6664c6ce6396a4d0cd413a50ce07 \
    --hash=sha256:213c60cd50106436cc818accf5baa1aba61c0189ff610f64f4a3e8c6726218ba \
    --hash=sha256:231710d57adfd809ef5d34183b8ed1eeae3f76459c18fb4a0b373ad56bedcdd9 \
    --hash=sha256:277a0ef2981ca40581a47093e9e2d13b3f1fbbeffae064c1d21bfceba2030287 \
    --hash=sha256:2cd5df3de48857ed0544b34e2d40e9fac445930039f3cfe4bcc592a1f836d513 \
    --hash=sha256:40527857252b61eacd1d9af500c3337ba8deb8fc298940291486c465c8b46ec0 \
    --hash=sha256:473f9edb243cb1935ab5a084eb238d842fb8f404ed2193a915d1784b5a6b5fc0 \
    --hash=sha256:48c346915c114f5fdb3ead70312bd042a953a8ce5c7106d5bfb1a5254e47da92 \
    --hash=sha256:50602afada6d6cbfad699b0c7bb50d5ccffa7e46a3d738092afddc1f9758427f \
    --hash=sha256:68fb519c14306fec9720a2a5b45bc9f0c8d1b9c72adf45c37baedfcd949c35a2 \
    --hash=sha256:77f396e6ef4c73fdc33a9157446466f1cff553d979bd00ecb64385760c6babdc \
    --hash=sha256:819b3830a1543db06c4d4b865e70ded25be52a2e0631ccd2f6a47a2822f2fd7c \
    --hash=sha256:897b80890765f037df3403d22bab41627ca8811ae55e9a722fd0392850ec4d86 \
    --hash=sha256:98c4d36e99714e55cfbaaee6dd5badbc9a1ec339ebfc3b1f52e293aee6bb71a4 \
    --hash=sha256:9df7ed3b3d2e0ecfe09e14741b857df43adb5a3ddadc919a2d94fbdf78fea53c \
    --hash=sha256:9fa600030013c4de8165339db93d182b9431076eb98eb40ee068700c9c813e34 \
    --hash=sha256:a80a78046a72361de73f8f395f1f1e49f956c6be882eed58505a15f3e430962b \
    --hash=sha256:b3d267842bf12586ba6c734f89d1f5b871df0273157918b0ccefa29deb05c21c \
    --hash=sha256:b5b9eccad747aabaaffbc6064800670f0c297e52c12754eb1d976c57e4f74dcb \
    --hash=sha256:c5687b8d43cf58545ade1fe3e055f70eac7a5a1a0bf42824308d868289a95737 \
    --hash=sha256:cba8c411ef271aa037d7357a2bc8f9ee8b58b9965831d9e51baf703280dc73d3 \
    --hash=sha256:d15a181d1ecd0d4270dc32edb46f7cb7733c7c508857278d3d378d14d606db2d \
    --hash=sha256:d4db7c7aef085872ef65a8fd7d6d09a14ae91f691dec3e87ee5ee0539d516f53 \
    --hash=sha256:d4eccecf9adf6fbcc6861a38015c2a64f38b9d94838ac1810a9023a0609e1b78 \
    --hash=sha256:d67d839ede4ed1b28a4e8909735fc992a923cdb84e618544973d7dfc71540803 \
    --hash=sha256:daf496c58a8c52083df09b80c860005194014c3698698d1a57cbcfa182142a3a \
    --hash=sha256:e61ceaab6f49fb8bdfaa0f92c4b57bcfbea54c09277b1b4f7ac376bfb7a7c174 \
    --hash=sha256:f84fbc98b019fef2ee9a1cb3ce93e3187a6df0b2538a651bfb890254ba9f90b5
    # via yamllint
requests==2.25.1 \
    --hash=sha256:27973dd4a904a4f13b263a19c866c13b92a39ed1c964655f025f3f8d3d75b804 \
    --hash=sha256:c210084e36a42ae6b9219e00e48287def368a26d03a048ddad7bfee44f75871e
    # via -r requirements.in
s3cmd==2.1.0 \
    --hash=sha256:49cd23d516b17974b22b611a95ce4d93fe326feaa07320bd1d234fed68cbccfa \
    --hash=sha256:966b0a494a916fc3b4324de38f089c86c70ee90e8e1cae6d59102103a4c0cc03
    # via -r requirements.in
six==1.16.0 \
    --hash=sha256:1e61c37477a1626458e36f7b1d82aa5c9b094fa4802892072e49de9c60c4c926 \
    --hash=sha256:8abb2f1d86890a2dfb989f9a77cfcfd3e47c2a354b01111771326f8aa26e0254
    # via python-dateutil
urllib3==1.26.7 \
    --hash=sha256:4987c65554f7a2dbf30c18fd48778ef124af6fab771a377103da0585e2336ece \
    --hash=sha256:c4fdf4019605b6e5423637e01bc9fe4daef873709a7973e195ceba0a62bbc844
    # via requests
yamllint==1.26.3 \
    --hash=sha256:3934dcde484374596d6b52d8db412929a169f6d9e52e20f9ade5bf3523d9b96e
    # via -r requirements.in

# The following packages are considered to be unsafe in a requirements file:
setuptools==59.6.0 \
    --hash=sha256:22c7348c6d2976a52632c67f7ab0cdf40147db7789f9aed18734643fe9cf3373 \
    --hash=sha256:4ce92f1e1f8f01233ee9952c04f6b81d1e02939d6e1b488428154974a4d0783e
    # via yamllint
""").requirements)

    return unittest.end(env)

parse_basic_test = unittest.make(
    _parse_basic_test_impl,
    attrs = {},
)
parse_requirements_lockfile_test = unittest.make(
    _parse_requirements_lockfile_test_impl,
    attrs = {},
)

def parse_requirements_txt_test_suite(name):
    unittest.suite(name, parse_basic_test, parse_requirements_lockfile_test)
