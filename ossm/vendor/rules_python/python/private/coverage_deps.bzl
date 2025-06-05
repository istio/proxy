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

"""Dependencies for coverage.py used by the hermetic toolchain.
"""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")
load("//python/private:version_label.bzl", "version_label")

# START: maintained by 'bazel run //tools/private/update_deps:update_coverage_deps <version>'
_coverage_deps = {
    "cp310": {
        "aarch64-apple-darwin": (
            "https://files.pythonhosted.org/packages/7d/73/041928e434442bd3afde5584bdc3f932fb4562b1597629f537387cec6f3d/coverage-7.6.1-cp310-cp310-macosx_11_0_arm64.whl",
            "cf4b19715bccd7ee27b6b120e7e9dd56037b9c0681dcc1adc9ba9db3d417fa36",
        ),
        "aarch64-unknown-linux-gnu": (
            "https://files.pythonhosted.org/packages/c7/c8/6ca52b5147828e45ad0242388477fdb90df2c6cbb9a441701a12b3c71bc8/coverage-7.6.1-cp310-cp310-manylinux_2_17_aarch64.manylinux2014_aarch64.whl",
            "e61c0abb4c85b095a784ef23fdd4aede7a2628478e7baba7c5e3deba61070a02",
        ),
        "x86_64-apple-darwin": (
            "https://files.pythonhosted.org/packages/7e/61/eb7ce5ed62bacf21beca4937a90fe32545c91a3c8a42a30c6616d48fc70d/coverage-7.6.1-cp310-cp310-macosx_10_9_x86_64.whl",
            "b06079abebbc0e89e6163b8e8f0e16270124c154dc6e4a47b413dd538859af16",
        ),
        "x86_64-unknown-linux-gnu": (
            "https://files.pythonhosted.org/packages/53/23/9e2c114d0178abc42b6d8d5281f651a8e6519abfa0ef460a00a91f80879d/coverage-7.6.1-cp310-cp310-manylinux_2_5_x86_64.manylinux1_x86_64.manylinux_2_17_x86_64.manylinux2014_x86_64.whl",
            "8f59d57baca39b32db42b83b2a7ba6f47ad9c394ec2076b084c3f029b7afca23",
        ),
    },
    "cp311": {
        "aarch64-apple-darwin": (
            "https://files.pythonhosted.org/packages/e1/0e/e52332389e057daa2e03be1fbfef25bb4d626b37d12ed42ae6281d0a274c/coverage-7.6.1-cp311-cp311-macosx_11_0_arm64.whl",
            "ed37bd3c3b063412f7620464a9ac1314d33100329f39799255fb8d3027da50d3",
        ),
        "aarch64-unknown-linux-gnu": (
            "https://files.pythonhosted.org/packages/aa/cd/766b45fb6e090f20f8927d9c7cb34237d41c73a939358bc881883fd3a40d/coverage-7.6.1-cp311-cp311-manylinux_2_17_aarch64.manylinux2014_aarch64.whl",
            "d85f5e9a5f8b73e2350097c3756ef7e785f55bd71205defa0bfdaf96c31616ff",
        ),
        "x86_64-apple-darwin": (
            "https://files.pythonhosted.org/packages/ad/5f/67af7d60d7e8ce61a4e2ddcd1bd5fb787180c8d0ae0fbd073f903b3dd95d/coverage-7.6.1-cp311-cp311-macosx_10_9_x86_64.whl",
            "7dea0889685db8550f839fa202744652e87c60015029ce3f60e006f8c4462c93",
        ),
        "x86_64-unknown-linux-gnu": (
            "https://files.pythonhosted.org/packages/14/6f/8351b465febb4dbc1ca9929505202db909c5a635c6fdf33e089bbc3d7d85/coverage-7.6.1-cp311-cp311-manylinux_2_5_x86_64.manylinux1_x86_64.manylinux_2_17_x86_64.manylinux2014_x86_64.whl",
            "0c0420b573964c760df9e9e86d1a9a622d0d27f417e1a949a8a66dd7bcee7bc6",
        ),
    },
    "cp312": {
        "aarch64-apple-darwin": (
            "https://files.pythonhosted.org/packages/e1/ab/6bf00de5327ecb8db205f9ae596885417a31535eeda6e7b99463108782e1/coverage-7.6.1-cp312-cp312-macosx_11_0_arm64.whl",
            "5621a9175cf9d0b0c84c2ef2b12e9f5f5071357c4d2ea6ca1cf01814f45d2391",
        ),
        "aarch64-unknown-linux-gnu": (
            "https://files.pythonhosted.org/packages/92/8f/2ead05e735022d1a7f3a0a683ac7f737de14850395a826192f0288703472/coverage-7.6.1-cp312-cp312-manylinux_2_17_aarch64.manylinux2014_aarch64.whl",
            "260933720fdcd75340e7dbe9060655aff3af1f0c5d20f46b57f262ab6c86a5e8",
        ),
        "x86_64-apple-darwin": (
            "https://files.pythonhosted.org/packages/7e/d4/300fc921dff243cd518c7db3a4c614b7e4b2431b0d1145c1e274fd99bd70/coverage-7.6.1-cp312-cp312-macosx_10_9_x86_64.whl",
            "95cae0efeb032af8458fc27d191f85d1717b1d4e49f7cb226cf526ff28179778",
        ),
        "x86_64-unknown-linux-gnu": (
            "https://files.pythonhosted.org/packages/1f/0f/c890339dd605f3ebc269543247bdd43b703cce6825b5ed42ff5f2d6122c7/coverage-7.6.1-cp312-cp312-manylinux_2_5_x86_64.manylinux1_x86_64.manylinux_2_17_x86_64.manylinux2014_x86_64.whl",
            "c44fee9975f04b33331cb8eb272827111efc8930cfd582e0320613263ca849ca",
        ),
    },
    "cp313": {
        "aarch64-apple-darwin": (
            "https://files.pythonhosted.org/packages/b9/67/e1413d5a8591622a46dd04ff80873b04c849268831ed5c304c16433e7e30/coverage-7.6.1-cp313-cp313-macosx_11_0_arm64.whl",
            "a6d3adcf24b624a7b778533480e32434a39ad8fa30c315208f6d3e5542aeb6e9",
        ),
        "aarch64-apple-darwin-freethreaded": (
            "https://files.pythonhosted.org/packages/c4/ae/b5d58dff26cade02ada6ca612a76447acd69dccdbb3a478e9e088eb3d4b9/coverage-7.6.1-cp313-cp313t-macosx_11_0_arm64.whl",
            "502753043567491d3ff6d08629270127e0c31d4184c4c8d98f92c26f65019962",
        ),
        "aarch64-unknown-linux-gnu": (
            "https://files.pythonhosted.org/packages/14/5b/9dec847b305e44a5634d0fb8498d135ab1d88330482b74065fcec0622224/coverage-7.6.1-cp313-cp313-manylinux_2_17_aarch64.manylinux2014_aarch64.whl",
            "d0c212c49b6c10e6951362f7c6df3329f04c2b1c28499563d4035d964ab8e08c",
        ),
        "aarch64-unknown-linux-gnu-freethreaded": (
            "https://files.pythonhosted.org/packages/b8/d7/62095e355ec0613b08dfb19206ce3033a0eedb6f4a67af5ed267a8800642/coverage-7.6.1-cp313-cp313t-manylinux_2_17_aarch64.manylinux2014_aarch64.whl",
            "6a89ecca80709d4076b95f89f308544ec8f7b4727e8a547913a35f16717856cb",
        ),
        "x86_64-unknown-linux-gnu": (
            "https://files.pythonhosted.org/packages/f7/95/d2fd31f1d638df806cae59d7daea5abf2b15b5234016a5ebb502c2f3f7ee/coverage-7.6.1-cp313-cp313-manylinux_2_5_x86_64.manylinux1_x86_64.manylinux_2_17_x86_64.manylinux2014_x86_64.whl",
            "78b260de9790fd81e69401c2dc8b17da47c8038176a79092a89cb2b7d945d060",
        ),
        "x86_64-unknown-linux-gnu-freethreaded": (
            "https://files.pythonhosted.org/packages/8b/61/a7a6a55dd266007ed3b1df7a3386a0d760d014542d72f7c2c6938483b7bd/coverage-7.6.1-cp313-cp313t-manylinux_2_5_x86_64.manylinux1_x86_64.manylinux_2_17_x86_64.manylinux2014_x86_64.whl",
            "13b0a73a0896988f053e4fbb7de6d93388e6dd292b0d87ee51d106f2c11b465b",
        ),
    },
    "cp38": {
        "aarch64-apple-darwin": (
            "https://files.pythonhosted.org/packages/38/ea/cab2dc248d9f45b2b7f9f1f596a4d75a435cb364437c61b51d2eb33ceb0e/coverage-7.6.1-cp38-cp38-macosx_11_0_arm64.whl",
            "f1adfc8ac319e1a348af294106bc6a8458a0f1633cc62a1446aebc30c5fa186a",
        ),
        "aarch64-unknown-linux-gnu": (
            "https://files.pythonhosted.org/packages/ca/6f/f82f9a500c7c5722368978a5390c418d2a4d083ef955309a8748ecaa8920/coverage-7.6.1-cp38-cp38-manylinux_2_17_aarch64.manylinux2014_aarch64.whl",
            "a95324a9de9650a729239daea117df21f4b9868ce32e63f8b650ebe6cef5595b",
        ),
        "x86_64-apple-darwin": (
            "https://files.pythonhosted.org/packages/81/d0/d9e3d554e38beea5a2e22178ddb16587dbcbe9a1ef3211f55733924bf7fa/coverage-7.6.1-cp38-cp38-macosx_10_9_x86_64.whl",
            "6db04803b6c7291985a761004e9060b2bca08da6d04f26a7f2294b8623a0c1a0",
        ),
        "x86_64-unknown-linux-gnu": (
            "https://files.pythonhosted.org/packages/e4/6e/885bcd787d9dd674de4a7d8ec83faf729534c63d05d51d45d4fa168f7102/coverage-7.6.1-cp38-cp38-manylinux_2_5_x86_64.manylinux1_x86_64.manylinux_2_17_x86_64.manylinux2014_x86_64.whl",
            "8929543a7192c13d177b770008bc4e8119f2e1f881d563fc6b6305d2d0ebe9de",
        ),
    },
    "cp39": {
        "aarch64-apple-darwin": (
            "https://files.pythonhosted.org/packages/a5/fe/137d5dca72e4a258b1bc17bb04f2e0196898fe495843402ce826a7419fe3/coverage-7.6.1-cp39-cp39-macosx_11_0_arm64.whl",
            "547f45fa1a93154bd82050a7f3cddbc1a7a4dd2a9bf5cb7d06f4ae29fe94eaf8",
        ),
        "aarch64-unknown-linux-gnu": (
            "https://files.pythonhosted.org/packages/78/5b/a0a796983f3201ff5485323b225d7c8b74ce30c11f456017e23d8e8d1945/coverage-7.6.1-cp39-cp39-manylinux_2_17_aarch64.manylinux2014_aarch64.whl",
            "645786266c8f18a931b65bfcefdbf6952dd0dea98feee39bd188607a9d307ed2",
        ),
        "x86_64-apple-darwin": (
            "https://files.pythonhosted.org/packages/19/d3/d54c5aa83268779d54c86deb39c1c4566e5d45c155369ca152765f8db413/coverage-7.6.1-cp39-cp39-macosx_10_9_x86_64.whl",
            "abd5fd0db5f4dc9289408aaf34908072f805ff7792632250dcb36dc591d24255",
        ),
        "x86_64-unknown-linux-gnu": (
            "https://files.pythonhosted.org/packages/9a/6f/eef79b779a540326fee9520e5542a8b428cc3bfa8b7c8f1022c1ee4fc66c/coverage-7.6.1-cp39-cp39-manylinux_2_5_x86_64.manylinux1_x86_64.manylinux_2_17_x86_64.manylinux2014_x86_64.whl",
            "609b06f178fe8e9f89ef676532760ec0b4deea15e9969bf754b37f7c40326dbc",
        ),
    },
}
# END: maintained by 'bazel run //tools/private/update_deps:update_coverage_deps <version>'

_coverage_patch = Label("//python/private:coverage.patch")

def coverage_dep(name, python_version, platform, visibility):
    """Register a single coverage dependency based on the python version and platform.

    Args:
        name: The name of the registered repository.
        python_version: The full python version.
        platform: The platform, which can be found in //python:versions.bzl PLATFORMS dict.
        visibility: The visibility of the coverage tool.

    Returns:
        The label of the coverage tool if the platform is supported, otherwise - None.
    """
    if "windows" in platform:
        # NOTE @aignas 2023-01-19: currently we do not support windows as the
        # upstream coverage wrapper is written in shell. Do not log any warning
        # for now as it is not actionable.
        return None

    abi = "cp" + version_label(python_version)
    url, sha256 = _coverage_deps.get(abi, {}).get(platform, (None, ""))

    if url == None:
        # Some wheels are not present for some builds, so let's silently ignore those.
        return None

    maybe(
        http_archive,
        name = name,
        build_file_content = """
filegroup(
    name = "coverage",
    srcs = ["coverage/__main__.py"],
    data = glob(["coverage/*.py", "coverage/**/*.py", "coverage/*.so"]),
    visibility = {visibility},
)
    """.format(
            visibility = visibility,
        ),
        patch_args = ["-p1"],
        patches = [_coverage_patch],
        sha256 = sha256,
        type = "zip",
        urls = [url],
    )

    return "@{name}//:coverage".format(name = name)
