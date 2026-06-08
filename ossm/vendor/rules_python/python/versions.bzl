# Copyright 2022 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""The Python versions we use for the toolchains.
"""

load("//python/private:platform_info.bzl", "platform_info")

# Values present in the @platforms//os package
MACOS_NAME = "osx"
LINUX_NAME = "linux"
WINDOWS_NAME = "windows"

FREETHREADED = "-freethreaded"
MUSL = "-musl"
INSTALL_ONLY = "install_only"

DEFAULT_RELEASE_BASE_URL = "https://github.com/astral-sh/python-build-standalone/releases/download"

# When updating the versions and releases, run the following command to get
# the hashes:
#   bazel run //python/private:print_toolchains_checksums --//python/config_settings:python_version={major}.{minor}.{patch}
#
# To print hashes for all of the specified versions, run:
#   bazel run //python/private:print_toolchains_checksums --//python/config_settings:python_version=""
#
# Note, to users looking at how to specify their tool versions, coverage_tool version for each
# interpreter can be specified by:
#   "3.8.10": {
#       "url": "20210506/cpython-{python_version}-{platform}-pgo+lto-20210506T0943.tar.zst",
#       "sha256": {
#           "x86_64-apple-darwin": "8d06bec08db8cdd0f64f4f05ee892cf2fcbc58cfb1dd69da2caab78fac420238",
#           "x86_64-unknown-linux-gnu": "aec8c4c53373b90be7e2131093caa26063be6d9d826f599c935c0e1042af3355",
#       },
#       "coverage_tool": {
#           "x86_64-apple-darwin": "<label_for_darwin>"",
#           "x86_64-unknown-linux-gnu": "<label_for_linux>"",
#       },
#       "strip_prefix": "python",
#   },
#
# It is possible to provide lists in "url". It is also possible to provide patches or patch_strip.
#
# buildifier: disable=unsorted-dict-items
TOOL_VERSIONS = {
    "3.9.25": {
        "url": "20251031/cpython-{python_version}+20251031-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "87275619c2706affa4d1090d2ca3dad354b6d69f8b85dbfafe38785870751b9a",
            "aarch64-unknown-linux-gnu": "6112d46355857680b81849764a6cf9f38cc4cd0d1cf29d432bc12fe5aeedf9d0",
            "ppc64le-unknown-linux-gnu": "828364b6f54fa45ac2dc91f8e45d5b74306372af374a9ef16eeb2ea81253ed3f",
            "riscv64-unknown-linux-gnu": "17467e0158e5ad04453c447d6773c23b044172276441e22e23058fd3ea053e27",
            "s390x-unknown-linux-gnu": "3e9539f83e67faa813fd06171199b2d33c89821dfa9a33bf6e27ad67f1b6932d",
            "x86_64-apple-darwin": "ace63cfe27a9487c4d72e1cb518be01c1d985271da0b2158e813801f7d3e5503",
            "x86_64-pc-windows-msvc": "4fb1b416482ce94d73cfa140317a670c596c830671d137b07c26afe8c461768a",
            "x86_64-unknown-linux-gnu": "42834f61eb6df43432c3dd6ab9ca3fdf8c06d10a404ebdb53d6902e6b9570b08",
            "x86_64-unknown-linux-musl": "76593e8c889e81e82db5fe117fe15b69466f85100ab2ec0e4035aa86242b4e93",
        },
        "strip_prefix": "python",
    },
    "3.10.2": {
        "url": "20220227/cpython-{python_version}+20220227-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "1409acd9a506e2d1d3b65c1488db4e40d8f19d09a7df099667c87a506f71c0ef",
            "aarch64-unknown-linux-gnu": "8f351a8cc348bb45c0f95b8634c8345ec6e749e483384188ad865b7428342703",
            "x86_64-apple-darwin": "8146ad4390710ec69b316a5649912df0247d35f4a42e2aa9615bffd87b3e235a",
            "x86_64-pc-windows-msvc": "a1d9a594cd3103baa24937ad9150c1a389544b4350e859200b3e5c036ac352bd",
            "x86_64-unknown-linux-gnu": "9b64eca2a94f7aff9409ad70bdaa7fbbf8148692662e764401883957943620dd",
        },
        "strip_prefix": "python",
    },
    "3.10.4": {
        "url": "20220502/cpython-{python_version}+20220502-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "2c99983d1e83e4b6e7411ed9334019f193fba626344a50c36fba6c25d4de78a2",
            "aarch64-unknown-linux-gnu": "d8098c0c54546637e7516f93b13403b11f9db285def8d7abd825c31407a13d7e",
            "x86_64-apple-darwin": "f2711eaffff3477826a401d09a013c6802f11c04c63ab3686aa72664f1216a05",
            "x86_64-pc-windows-msvc": "bee24a3a5c83325215521d261d73a5207ab7060ef3481f76f69b4366744eb81d",
            "x86_64-unknown-linux-gnu": "f6f871e53a7b1469c13f9bd7920ad98c4589e549acad8e5a1e14760fff3dd5c9",
        },
        "strip_prefix": "python",
    },
    "3.10.6": {
        "url": "20220802/cpython-{python_version}+20220802-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "efaf66acdb9a4eb33d57702607d2e667b1a319d58c167a43c96896b97419b8b7",
            "aarch64-unknown-linux-gnu": "81625f5c97f61e2e3d7e9f62c484b1aa5311f21bd6545451714b949a29da5435",
            "x86_64-apple-darwin": "7718411adf3ea1480f3f018a643eb0550282aefe39e5ecb3f363a4a566a9398c",
            "x86_64-pc-windows-msvc": "91889a7dbdceea585ff4d3b7856a6bb8f8a4eca83a0ff52a73542c2e67220eaa",
            "x86_64-unknown-linux-gnu": "55aa2190d28dcfdf414d96dc5dcea9fe048fadcd583dc3981fec020869826111",
        },
        "strip_prefix": "python",
    },
    "3.10.8": {
        "url": "20221106/cpython-{python_version}+20221106-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "d52b03817bd245d28e0a8b2f715716cd0fcd112820ccff745636932c76afa20a",
            "aarch64-unknown-linux-gnu": "33170bef18c811906b738be530f934640491b065bf16c4d276c6515321918132",
            "x86_64-apple-darwin": "525b79c7ce5de90ab66bd07b0ac1008bafa147ddc8a41bef15ffb7c9c1e9e7c5",
            "x86_64-pc-windows-msvc": "f2b6d2f77118f06dd2ca04dae1175e44aaa5077a5ed8ddc63333c15347182bfe",
            "x86_64-unknown-linux-gnu": "6c8db44ae0e18e320320bbaaafd2d69cde8bfea171ae2d651b7993d1396260b7",
        },
        "strip_prefix": "python",
    },
    "3.10.9": {
        "url": "20230116/cpython-{python_version}+20230116-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "018d05a779b2de7a476f3b3ff2d10f503d69d14efcedd0774e6dab8c22ef84ff",
            "aarch64-unknown-linux-gnu": "2003750f40cd09d4bf7a850342613992f8d9454f03b3c067989911fb37e7a4d1",
            "x86_64-apple-darwin": "0e685f98dce0e5bc8da93c7081f4e6c10219792e223e4b5886730fd73a7ba4c6",
            "x86_64-pc-windows-msvc": "59c6970cecb357dc1d8554bd0540eb81ee7f6d16a07acf3d14ed294ece02c035",
            "x86_64-unknown-linux-gnu": "d196347aeb701a53fe2bb2b095abec38d27d0fa0443f8a1c2023a1bed6e18cdf",
        },
        "strip_prefix": "python",
    },
    "3.10.11": {
        "url": "20230507/cpython-{python_version}+20230507-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "8348bc3c2311f94ec63751fb71bd0108174be1c4def002773cf519ee1506f96f",
            "aarch64-unknown-linux-gnu": "c7573fdb00239f86b22ea0e8e926ca881d24fde5e5890851339911d76110bc35",
            "ppc64le-unknown-linux-gnu": "73a9d4c89ed51be39dd2de4e235078281087283e9fdedef65bec02f503e906ee",
            "x86_64-apple-darwin": "bd3fc6e4da6f4033ebf19d66704e73b0804c22641ddae10bbe347c48f82374ad",
            "x86_64-pc-windows-msvc": "9c2d3604a06fcd422289df73015cd00e7271d90de28d2c910f0e2309a7f73a68",
            "x86_64-unknown-linux-gnu": "c5bcaac91bc80bfc29cf510669ecad12d506035ecb3ad85ef213416d54aecd79",
        },
        "strip_prefix": "python",
    },
    "3.10.12": {
        "url": "20230726/cpython-{python_version}+20230726-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "bc66c706ea8c5fc891635fda8f9da971a1a901d41342f6798c20ad0b2a25d1d6",
            "aarch64-unknown-linux-gnu": "fee80e221663eca5174bd794cb5047e40d3910dbeadcdf1f09d405a4c1c15fe4",
            "ppc64le-unknown-linux-gnu": "bb5e8cb0d2e44241725fa9b342238245503e7849917660006b0246a9c97b1d6c",
            "s390x-unknown-linux-gnu": "8d33d435ae6fb93ded7fc26798cc0a1a4f546a4e527012a1e2909cc314b332df",
            "x86_64-apple-darwin": "8a6e3ed973a671de468d9c691ed9cb2c3a4858c5defffcf0b08969fba9c1dd04",
            "x86_64-pc-windows-msvc": "c1a31c353ca44de7d1b1a3b6c55a823e9c1eed0423d4f9f66e617bdb1b608685",
            "x86_64-unknown-linux-gnu": "a476dbca9184df9fc69fe6309cda5ebaf031d27ca9e529852437c94ec1bc43d3",
        },
        "strip_prefix": "python",
    },
    "3.10.13": {
        "url": "20240224/cpython-{python_version}+20240224-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "5fdc0f6a5b5a90fd3c528e8b1da8e3aac931ea8690126c2fdb4254c84a3ff04a",
            "aarch64-unknown-linux-gnu": "a898a88705611b372297bb8fe4d23cc16b8603ce5f24494c3a8cfa65d83787f9",
            "ppc64le-unknown-linux-gnu": "c23706e138a0351fc1e9def2974af7b8206bac7ecbbb98a78f5aa9e7535fee42",
            "s390x-unknown-linux-gnu": "09be8fb2cdfbb4a93d555f268f244dbe4d8ff1854b2658e8043aa4ec08aede3e",
            "x86_64-apple-darwin": "6378dfd22f58bb553ddb02be28304d739cd730c1f95c15c74955c923a1bc3d6a",
            "x86_64-pc-windows-msvc": "086f7fe9156b897bb401273db8359017104168ac36f60f3af4e31ac7acd6634e",
            "x86_64-unknown-linux-gnu": "d995d032ca702afd2fc3a689c1f84a6c64972ecd82bba76a61d525f08eb0e195",
        },
        "strip_prefix": "python",
    },
    "3.10.14": {
        "url": "20240726/cpython-{python_version}+20240726-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "164d89f0df2feb689981864ecc1dffb19e6aa3696c8880166de555494fe92607",
            "aarch64-unknown-linux-gnu": "39bcd46b4d70e40da177c55259be16d5c2be7a3f7f93f1e3bde47e71b4833f29",
            "ppc64le-unknown-linux-gnu": "549d38b9ef59cba9ab2990025255231bfa1cb32b4bc5eac321667640fdee19d1",
            "s390x-unknown-linux-gnu": "de4bc878a8666c734f983db971610980870148f333bda8b0c34abfaeae88d7ec",
            "x86_64-apple-darwin": "1a1455838cd1e8ed0da14a152a2d559a2fd3a6047ba7013e841db4a35a228c1d",
            "x86_64-pc-windows-msvc": "7f68821a8b5445267eca480660364ebd06ec84632b336770c6e39de07ac0f6c3",
            "x86_64-unknown-linux-gnu": "32b34cd13d9d745b3db3f3b8398ab2c07de74544829915dbebd8dce39bdc405e",
        },
        "strip_prefix": "python",
    },
    "3.10.15": {
        "url": "20241016/cpython-{python_version}+20241016-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "f64776f455a44c24d50f947c813738cfb7b9ac43732c44891bc831fa7940a33c",
            "aarch64-unknown-linux-gnu": "eb58581f85fde83d1f3e8e1f8c6f5a15c7ae4fdbe3b1d1083931f9167fdd8dbc",
            "ppc64le-unknown-linux-gnu": "0c45af4e7525e2db59901606db32b2896ac1e9830c6f95551402207f537c2ce4",
            "s390x-unknown-linux-gnu": "de205896b070e6f5259ac0f2b3379eead875ea84e6a6ef533b89886fcbb46a4c",
            "x86_64-apple-darwin": "90b46dfb1abd98d45663c7a2a8c45d3047a59391d8586d71b459cec7b75f662b",
            "x86_64-pc-windows-msvc": "e48952619796c66ec9719867b87be97edca791c2ef7fbf87d42c417c3331609e",
            "x86_64-unknown-linux-gnu": "3db2171e03c1a7acdc599fba583c1b92306d3788b375c9323077367af1e9d9de",
            "x86_64-unknown-linux-musl": "ed519c47d9620eb916a6f95ec2875396e7b1a9ab993ee40b2f31b837733f318c",
        },
        "strip_prefix": "python",
    },
    "3.10.16": {
        "url": "20250317/cpython-{python_version}+20250317-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "e99f8457d9c79592c036489c5cfa78df76e4762d170665e499833e045d82608f",
            "aarch64-unknown-linux-gnu": "76d0f04d2444e77200fdc70d1c57480e29cca78cb7420d713bc1c523709c198d",
            "ppc64le-unknown-linux-gnu": "39c9b3486de984fe1d72d90278229c70d6b08bcf69cd55796881b2d75077b603",
            "riscv64-unknown-linux-gnu": "ebe949ada9293581c17d9bcdaa8f645f67d95f73eac65def760a71ef9dd6600d",
            "s390x-unknown-linux-gnu": "9b2fc0b7f1c75b48e799b6fa14f7e24f5c61f2db82e3c65d13ed25e08f7f0857",
            "x86_64-apple-darwin": "e03e62dbe95afa2f56b7344ff3bd061b180a0b690ff77f9a1d7e6601935e05ca",
            "x86_64-pc-windows-msvc": "c7e0eb0ff5b36758b7a8cacd42eb223c056b9c4d36eded9bf5b9fe0c0b9aeb08",
            "x86_64-unknown-linux-gnu": "b350c7e63956ca8edb856b91316328e0fd003a840cbd63d08253af43b2c63643",
            "x86_64-unknown-linux-musl": "6ed64923ee4fbea4c5780f1a5a66651d239191ac10bd23420db4f5e4e0bf79c4",
        },
        "strip_prefix": "python",
    },
    "3.10.18": {
        "url": "20250808/cpython-{python_version}+20250808-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "a94c02b2d597cd6b075a713fe4e9a909cc97ca6a3b2b2ce86eda21be2062d48e",
            "aarch64-unknown-linux-gnu": "ef7de3b715d519e246d98ff7856247f7f7b357068705f09c6f300b7e7b76c701",
            "ppc64le-unknown-linux-gnu": "f580efed11cc54e1a221c052e8bc88bfbc12844d3ca8949da828351a1232386e",
            "riscv64-unknown-linux-gnu": "0d7e460e30203a9225b6f417ae972f66415a1cc0e32b37ebc48d195816282669",
            "s390x-unknown-linux-gnu": "d4ada974daadb08a0184c19232ee3b03b3137aa70609760e1a94aaf7b12989ef",
            "x86_64-apple-darwin": "da96fe2ba841640215788ddb9f151f03629360e37fcb94d4f76e5095b87df0d4",
            "x86_64-pc-windows-msvc": "a648f3c9d136985ccfe57a5507e73d9d0839f7fd09eebd7c247857f2feaecb2a",
            "x86_64-unknown-linux-gnu": "0b310a73bb9e7a495dbcad5f685e508ca2e7b36ee8f29301a52285730c425789",
            "x86_64-unknown-linux-musl": "9cecf6ea2effbe183faebcf7e1160425a4ee17a68e49f2eefe5e1c59c51fa7ee",
        },
        "strip_prefix": "python",
    },
    "3.10.19": {
        "url": "20251031/cpython-{python_version}+20251031-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "43bda24c2fc073bc308bf631203b917a72640d59b59fdad4ba14503d84727012",
            "aarch64-unknown-linux-gnu": "f77a8a8aa77f3f943126fa9215a25309da4bf20398fc8f4b4eec54b5fc7570ef",
            "ppc64le-unknown-linux-gnu": "1c55d160fc4c3b93528cd6aaa2bb4ca6018a99e5a45919d33dc761a43a69f860",
            "riscv64-unknown-linux-gnu": "21134d35721cdad4c881f35d0957cc19df9a45d194afb38a099faded3c1cfb4d",
            "s390x-unknown-linux-gnu": "df0db070f1eb73ab4e371eea32213ddb3500737ea5560a6f0ffd65c82af64ddc",
            "x86_64-apple-darwin": "76c12e633c09c2a790f8a958a55df4495527e0718d1875310c836e757c0c7b55",
            "x86_64-pc-windows-msvc": "cfa08a4caf2df1b43551b843c052d6a8814e2ea0c97268b021f0423646c244c3",
            "x86_64-unknown-linux-gnu": "fb1caac917d7b6497bb6f5950da5f1e48d05c43a498948dd97f85760c4382d9f",
            "x86_64-unknown-linux-musl": "ba85013ed5ac7733fc6840168cc33ed19e9959b363dc80227d54f8fd9c92c0f4",
        },
        "strip_prefix": "python",
    },
    "3.11.1": {
        "url": "20230116/cpython-{python_version}+20230116-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "4918cdf1cab742a90f85318f88b8122aeaa2d04705803c7b6e78e81a3dd40f80",
            "aarch64-unknown-linux-gnu": "debf15783bdcb5530504f533d33fda75a7b905cec5361ae8f33da5ba6599f8b4",
            "x86_64-apple-darwin": "20a4203d069dc9b710f70b09e7da2ce6f473d6b1110f9535fb6f4c469ed54733",
            "x86_64-pc-windows-msvc": "edc08979cb0666a597466176511529c049a6f0bba8adf70df441708f766de5bf",
            "x86_64-unknown-linux-gnu": "02a551fefab3750effd0e156c25446547c238688a32fabde2995c941c03a6423",
        },
        "strip_prefix": "python",
    },
    "3.11.3": {
        "url": "20230507/cpython-{python_version}+20230507-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "09e412506a8d63edbb6901742b54da9aa7faf120b8dbdce56c57b303fc892c86",
            "aarch64-unknown-linux-gnu": "8190accbbbbcf7620f1ff6d668e4dd090c639665d11188ce864b62554d40e5ab",
            "ppc64le-unknown-linux-gnu": "767d24f3570b35fedb945f5ac66224c8983f2d556ab83c5cfaa5f3666e9c212c",
            "x86_64-apple-darwin": "f710b8d60621308149c100d5175fec39274ed0b9c99645484fd93d1716ef4310",
            "x86_64-pc-windows-msvc": "24741066da6f35a7ff67bee65ce82eae870d84e1181843e64a7076d1571e95af",
            "x86_64-unknown-linux-gnu": "da50b87d1ec42b3cb577dfd22a3655e43a53150f4f98a4bfb40757c9d7839ab5",
        },
        "strip_prefix": "python",
    },
    "3.11.4": {
        "url": "20230726/cpython-{python_version}+20230726-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "cb6d2948384a857321f2aa40fa67744cd9676a330f08b6dad7070bda0b6120a4",
            "aarch64-unknown-linux-gnu": "2e84fc53f4e90e11963281c5c871f593abcb24fc796a50337fa516be99af02fb",
            "ppc64le-unknown-linux-gnu": "df7b92ed9cec96b3bb658fb586be947722ecd8e420fb23cee13d2e90abcfcf25",
            "s390x-unknown-linux-gnu": "e477f0749161f9aa7887964f089d9460a539f6b4a8fdab5166f898210e1a87a4",
            "x86_64-apple-darwin": "47e1557d93a42585972772e82661047ca5f608293158acb2778dccf120eabb00",
            "x86_64-pc-windows-msvc": "878614c03ea38538ae2f758e36c85d2c0eb1eaaca86cd400ff8c76693ee0b3e1",
            "x86_64-unknown-linux-gnu": "e26247302bc8e9083a43ce9e8dd94905b40d464745b1603041f7bc9a93c65d05",
        },
        "strip_prefix": "python",
    },
    "3.11.5": {
        "url": "20230826/cpython-{python_version}+20230826-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "dab64b3580118ad2073babd7c29fd2053b616479df5c107d31fe2af1f45e948b",
            "aarch64-unknown-linux-gnu": "bb5c5d1ea0f199fe2d3f0996fff4b48ca6ddc415a3dbd98f50bff7fce48aac80",
            "ppc64le-unknown-linux-gnu": "14121b53e9c8c6d0741f911ae00102a35adbcf5c3cdf732687ef7617b7d7304d",
            "s390x-unknown-linux-gnu": "fe459da39874443579d6fe88c68777c6d3e331038e1fb92a0451879fb6beb16d",
            "x86_64-apple-darwin": "4a4efa7378c72f1dd8ebcce1afb99b24c01b07023aa6b8fea50eaedb50bf2bfc",
            "x86_64-pc-windows-msvc": "00f002263efc8aea896bcfaaf906b1f4dab3e5cd3db53e2b69ab9a10ba220b97",
            "x86_64-unknown-linux-gnu": "fbed6f7694b2faae5d7c401a856219c945397f772eea5ca50c6eb825cbc9d1e1",
        },
        "strip_prefix": "python",
    },
    "3.11.6": {
        "url": "20231002/cpython-{python_version}+20231002-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "916c35125b5d8323a21526d7a9154ca626453f63d0878e95b9f613a95006c990",
            "aarch64-unknown-linux-gnu": "3e26a672df17708c4dc928475a5974c3fb3a34a9b45c65fb4bd1e50504cc84ec",
            "ppc64le-unknown-linux-gnu": "7937035f690a624dba4d014ffd20c342e843dd46f89b0b0a1e5726b85deb8eaf",
            "s390x-unknown-linux-gnu": "f9f19823dba3209cedc4647b00f46ed0177242917db20fb7fb539970e384531c",
            "x86_64-apple-darwin": "178cb1716c2abc25cb56ae915096c1a083e60abeba57af001996e8bc6ce1a371",
            "x86_64-pc-windows-msvc": "3933545e6d41462dd6a47e44133ea40995bc6efeed8c2e4cbdf1a699303e95ea",
            "x86_64-unknown-linux-gnu": "ee37a7eae6e80148c7e3abc56e48a397c1664f044920463ad0df0fc706eacea8",
        },
        "strip_prefix": "python",
    },
    "3.11.7": {
        "url": "20240107/cpython-{python_version}+20240107-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "b042c966920cf8465385ca3522986b12d745151a72c060991088977ca36d3883",
            "aarch64-unknown-linux-gnu": "b102eaf865eb715aa98a8a2ef19037b6cc3ae7dfd4a632802650f29de635aa13",
            "ppc64le-unknown-linux-gnu": "b44e1b74afe75c7b19143413632c4386708ae229117f8f950c2094e9681d34c7",
            "s390x-unknown-linux-gnu": "49520e3ff494708020f306e30b0964f079170be83e956be4504f850557378a22",
            "x86_64-apple-darwin": "a0e615eef1fafdc742da0008425a9030b7ea68a4ae4e73ac557ef27b112836d4",
            "x86_64-pc-windows-msvc": "67077e6fa918e4f4fd60ba169820b00be7c390c497bf9bc9cab2c255ea8e6f3e",
            "x86_64-unknown-linux-gnu": "4a51ce60007a6facf64e5495f4cf322e311ba9f39a8cd3f3e4c026eae488e140",
        },
        "strip_prefix": "python",
    },
    "3.11.8": {
        "url": "20240224/cpython-{python_version}+20240224-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "389a51139f5abe071a0d70091ca5df3e7a3dfcfcbe3e0ba6ad85fb4c5638421e",
            "aarch64-unknown-linux-gnu": "389b9005fb78dd5a6f68df5ea45ab7b30d9a4b3222af96999e94fd20d4ad0c6a",
            "ppc64le-unknown-linux-gnu": "eb2b31f8e50309aae493c6a359c32b723a676f07c641f5e8fe4b6aa4dbb50946",
            "s390x-unknown-linux-gnu": "844f64f4c16e24965778281da61d1e0e6cd1358a581df1662da814b1eed096b9",
            "x86_64-apple-darwin": "097f467b0c36706bfec13f199a2eaf924e668f70c6e2bd1f1366806962f7e86e",
            "x86_64-pc-windows-msvc": "b618f1f047349770ee1ef11d1b05899840abd53884b820fd25c7dfe2ec1664d4",
            "x86_64-unknown-linux-gnu": "94e13d0e5ad417035b80580f3e893a72e094b0900d5d64e7e34ab08e95439987",
        },
        "strip_prefix": "python",
    },
    "3.11.9": {
        "url": "20240726/cpython-{python_version}+20240726-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "cbdac9462bab9671c8e84650e425d3f43b775752a930a2ef954a0d457d5c00c3",
            "aarch64-unknown-linux-gnu": "4d17cf988abe24449d649aad3ef974091ab76807904d41839907061925b4c9e3",
            "ppc64le-unknown-linux-gnu": "fc4f3c9ef9bfac2ed0282126ff376e544697ad04a5408d6429d46899d7d3bf21",
            "s390x-unknown-linux-gnu": "e69b66e53e926460df044f44846eef3fea642f630e829719e1a4112fc370dc56",
            "x86_64-apple-darwin": "dc3174666a30f4c38d04e79a80c3159b4b3aa69597c4676701c8386696811611",
            "x86_64-pc-windows-msvc": "f694be48bdfec1dace6d69a19906b6083f4dd7c7c61f1138ba520e433e5598f8",
            "x86_64-unknown-linux-gnu": "f6e955dc9ddfcad74e77abe6f439dac48ebca14b101ed7c85a5bf3206ed2c53d",
        },
        "strip_prefix": "python",
    },
    "3.11.10": {
        "url": "20241016/cpython-{python_version}+20241016-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "5a69382da99c4620690643517ca1f1f53772331b347e75f536088c42a4cf6620",
            "aarch64-unknown-linux-gnu": "803e49259280af0f5466d32829cd9d65a302b0226e424b3f0b261f9daf6aee8f",
            "ppc64le-unknown-linux-gnu": "92b666d103902001322f42badbd68da92adc5cebb826af9c1c906c33166e2f34",
            "s390x-unknown-linux-gnu": "6d584317651c1ad4a857cb32d1999707e8bb3046fcb2f156d80381814fa19fde",
            "x86_64-apple-darwin": "1e23ffe5bc473e1323ab8f51464da62d77399afb423babf67f8e13c82b69c674",
            "x86_64-pc-windows-msvc": "647b66ff4552e70aec3bf634dd470891b4a2b291e8e8715b3bdb162f577d4c55",
            "x86_64-unknown-linux-gnu": "8b50a442b04724a24c1eebb65a36a0c0e833d35374dbdf9c9470d8a97b164cd9",
            "x86_64-unknown-linux-musl": "d36fc77a8dd76155a7530f6235999a693b9e7c48aa11afeb5610a091cae5aa6f",
        },
        "strip_prefix": "python",
    },
    "3.11.13": {
        "url": "20250808/cpython-{python_version}+20250808-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "d089bfd2c7b98a0942750a195e70d3172beda76d7747097b8afd87028b6e59b6",
            "aarch64-unknown-linux-gnu": "bc57105f8a16acd57b71d926143c7f6ecf61729b40c8b4656f1b98bebd47c710",
            "ppc64le-unknown-linux-gnu": "16a0165b0744940702b8fff80b8bf973ac914f78cb6fca28d389583f675e84de",
            "riscv64-unknown-linux-gnu": "d8e62306be8f41c46bcd62ca68f91a1467f47adff632a35ff413dc1043ed56e8",
            "s390x-unknown-linux-gnu": "4e302a4514a73baefdd9b327062bdafeb4115a799deec91c185f6ab45a857241",
            "x86_64-apple-darwin": "d946d618f8bba8308b67e460a30612a71e2ccc309f85f6628aaae24e2b816981",
            "x86_64-pc-windows-msvc": "ed963aee33d29ad8abfbb5fe63e42f57a2638a4a11a88e11d8bb66e61f20a6e5",
            "aarch64-pc-windows-msvc": "a632857c966237e7fd38b44c47c350f6e30d8ec54dcad6c832865ad670f0f22f",
            "x86_64-unknown-linux-gnu": "3ad988c702cbb017fef1208d47dea4138a2e85fd0f7f01ec5e1e335e597131b9",
            "x86_64-unknown-linux-musl": "3a5810f0696f844289aa06d5c3a1efeab66eee999c25196b7d1954192a2c2100",
        },
        "strip_prefix": "python",
    },
    "3.11.14": {
        "url": "20251031/cpython-{python_version}+20251031-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "6de5572b33c65af1c9b7caf00ec593fb04cffb7e14fa393a98261bb9bc464713",
            "aarch64-unknown-linux-gnu": "510edb027527413c4249256194cb8ad2590b52dd93f7123b4cb341aff5d05894",
            "ppc64le-unknown-linux-gnu": "4e0bc6a818e0c6a9d7d3ebe1a95591fd84440520577aa837facc96a4b7a80e35",
            "riscv64-unknown-linux-gnu": "16519e69297144f81b2421333bc9e0b6466cf3c84749b216b695cfb4c9deb32f",
            "s390x-unknown-linux-gnu": "5f9c1b203cdf34c8bff1aef69b63bbf11309bd16ca6e429d8c3651eaa2b3d080",
            "x86_64-apple-darwin": "4891cbf34e8652b7bd1054b9502395e4b7e048e2e517c040fbf6c8297cb954d6",
            "x86_64-pc-windows-msvc": "5223b83ed9e2aa5e9e17d2ebcf767956e998876339b9cde1980a47e9d4655fb6",
            "aarch64-pc-windows-msvc": "38d0d1466561e15965e8d2c20f5e5be649598f55c761ecab553d087fbd217337",
            "x86_64-unknown-linux-gnu": "60f0bd473d861cc45d3401d9914e47ccb9fa037f88a91879ed517a62042b8477",
            "x86_64-unknown-linux-musl": "25e82d1e85b90a8ab724ee633a1811b1921797f5c25ee69c6595052371b91a87",
        },
        "strip_prefix": "python",
    },
    "3.12.0": {
        "url": "20231002/cpython-{python_version}+20231002-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "4734a2be2becb813830112c780c9879ac3aff111a0b0cd590e65ec7465774d02",
            "aarch64-unknown-linux-gnu": "bccfe67cf5465a3dfb0336f053966e2613a9bc85a6588c2fcf1366ef930c4f88",
            "ppc64le-unknown-linux-gnu": "b5dae075467ace32c594c7877fe6ebe0837681f814601d5d90ba4c0dfd87a1f2",
            "s390x-unknown-linux-gnu": "5681621349dd85d9726d1b67c84a9686ce78f72e73a6f9e4cc4119911655759e",
            "x86_64-apple-darwin": "5a9e88c8aa52b609d556777b52ebde464ae4b4f77e4aac4eb693af57395c9abf",
            "x86_64-pc-windows-msvc": "facfaa1fbc8653f95057f3c4a0f8aa833dab0e0b316e24ee8686bc761d4b4f8d",
            "x86_64-unknown-linux-gnu": "e51a5293f214053ddb4645b2c9f84542e2ef86870b8655704367bd4b29d39fe9",
        },
        "strip_prefix": "python",
    },
    "3.12.1": {
        "url": "20240107/cpython-{python_version}+20240107-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "f93f8375ca6ac0a35d58ff007043cbd3a88d9609113f1cb59cf7c8d215f064af",
            "aarch64-unknown-linux-gnu": "236533ef20e665007a111c2f36efb59c87ae195ad7dca223b6dc03fb07064f0b",
            "ppc64le-unknown-linux-gnu": "78051f0d1411ee62bc2af5edfccf6e8400ac4ef82887a2affc19a7ace6a05267",
            "s390x-unknown-linux-gnu": "60631211c701f8d2c56e5dd7b154e68868128a019b9db1d53a264f56c0d4aee2",
            "x86_64-apple-darwin": "eca96158c1568dedd9a0b3425375637a83764d1fa74446438293089a8bfac1f8",
            "x86_64-pc-windows-msvc": "fd5a9e0f41959d0341246d3643f2b8794f638adc0cec8dd5e1b6465198eae08a",
            "x86_64-unknown-linux-gnu": "74e330b8212ca22fd4d9a2003b9eec14892155566738febc8e5e572f267b9472",
        },
        "strip_prefix": "python",
    },
    "3.12.2": {
        "url": "20240224/cpython-{python_version}+20240224-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "01c064c00013b0175c7858b159989819ead53f4746d40580b5b0b35b6e80fba6",
            "aarch64-unknown-linux-gnu": "e52550379e7c4ac27a87de832d172658bc04150e4e27d4e858e6d8cbb96fd709",
            "ppc64le-unknown-linux-gnu": "74bc02c4bbbd26245c37b29b9e12d0a9c1b7ab93477fed8b651c988b6a9a6251",
            "s390x-unknown-linux-gnu": "ecd6b0285e5eef94deb784b588b4b425a15a43ae671bf206556659dc141a9825",
            "x86_64-apple-darwin": "a53a6670a202c96fec0b8c55ccc780ea3af5307eb89268d5b41a9775b109c094",
            "x86_64-pc-windows-msvc": "1e5655a6ccb1a64a78460e4e3ee21036c70246800f176a6c91043a3fe3654a3b",
            "x86_64-unknown-linux-gnu": "57a37b57f8243caa4cdac016176189573ad7620f0b6da5941c5e40660f9468ab",
        },
        "strip_prefix": "python",
    },
    "3.12.3": {
        "url": "20240415/cpython-{python_version}+20240415-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "ccc40e5af329ef2af81350db2a88bbd6c17b56676e82d62048c15d548401519e",
            "aarch64-unknown-linux-gnu": "ec8126de97945e629cca9aedc80a29c4ae2992c9d69f2655e27ae73906ba187d",
            "ppc64le-unknown-linux-gnu": "c5dcf08b8077e617d949bda23027c49712f583120b3ed744f9b143da1d580572",
            "s390x-unknown-linux-gnu": "872fc321363b8cdd826fd2cb1adfd1ceb813bc1281f9d410c1c2c4e177e8df86",
            "x86_64-apple-darwin": "c37a22fca8f57d4471e3708de6d13097668c5f160067f264bb2b18f524c890c8",
            "x86_64-pc-windows-msvc": "f7cfa4ad072feb4578c8afca5ba9a54ad591d665a441dd0d63aa366edbe19279",
            "x86_64-unknown-linux-gnu": "a73ba777b5d55ca89edef709e6b8521e3f3d4289581f174c8699adfb608d09d6",
        },
        "strip_prefix": "python",
    },
    "3.12.4": {
        "url": "20240726/cpython-{python_version}+20240726-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "1801025e825c04b3907e4ef6220a13607bc0397628c9485897073110ef7fde15",
            "aarch64-unknown-linux-gnu": "a098b18b7e9fea0c66867b76c0124fce9465765017572b2e7b522154c87c78d7",
            "ppc64le-unknown-linux-gnu": "04011c4c5b7fe34b0b895edf4ad8748e410686c1d69aaee11d6688d481023bcb",
            "s390x-unknown-linux-gnu": "8f8f3e29cf0c2facdbcfee70660939fda7667ac24fee8656d3388fc72f3acc7c",
            "x86_64-apple-darwin": "4c325838c1b0ed13698506fcd515be25c73dcbe195f8522cf98f9148a97601ed",
            "x86_64-pc-windows-msvc": "74309b0f322716409883d38c621743ea7fa0376eb00927b8ee1e1671d3aff450",
            "x86_64-unknown-linux-gnu": "e133dd6fc6a2d0033e2658637cc22e9c95f9d7073b80115037ee1f16417a54ac",
        },
        "strip_prefix": "python",
    },
    "3.12.7": {
        "url": "20241016/cpython-{python_version}+20241016-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "4c18852bf9c1a11b56f21bcf0df1946f7e98ee43e9e4c0c5374b2b3765cf9508",
            "aarch64-unknown-linux-gnu": "bba3c6be6153f715f2941da34f3a6a69c2d0035c9c5396bc5bb68c6d2bd1065a",
            "ppc64le-unknown-linux-gnu": "0a1d1d92e33a969bd2f40a80af53c97b6c0cc1060d384ceff50ff801593bf9d6",
            "s390x-unknown-linux-gnu": "935676a0c960b552f95e9ac2e1e385de5de4b34038ff65ffdc688838f1189c17",
            "x86_64-apple-darwin": "60c5271e7edc3c2ab47440b7abf4ed50fbc693880b474f74f05768f5b657045a",
            "x86_64-pc-windows-msvc": "f05531bff16fa77b53be0776587b97b466070e768e6d5920894de988bdcd547a",
            "x86_64-unknown-linux-gnu": "43576f7db1033dd57b900307f09c2e86f371152ac8a2607133afa51cbfc36064",
            "x86_64-unknown-linux-musl": "5ed4a4078db3cbac563af66403aaa156cd6e48831d90382a1820db2b120627b5",
        },
        "strip_prefix": "python",
    },
    "3.12.8": {
        "url": "20241206/cpython-{python_version}+20241206-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "e3c4aa607717b23903ca2650d5c3ee24f89b97543e2db2b0f463bddc7a9e92f3",
            "aarch64-unknown-linux-gnu": "ce674b55442b732973afb2932c281bb1ded4ad7e22bcf9b07071165770758c7e",
            "ppc64le-unknown-linux-gnu": "b7214790b273de9ed0532420054b72ba1393d62d2fc844ec55ade193771bd90c",
            "s390x-unknown-linux-gnu": "73102f5dbd7d1e7e9c2f2c80aedf2893d99a7fa407f6674ec8b2f57ba07daee5",
            "x86_64-apple-darwin": "3ba35c706577d755e8e52a4c161a042464577c0e695e2a605362fa469e26de10",
            "x86_64-pc-windows-msvc": "767b4be3ddf6b99e5ade519789c1615c191d8cf99d5aff4685cc18b48931f1e6",
            "x86_64-unknown-linux-gnu": "b9d6ee5ddac1198e72d53112698773fc8bb597de095592eb849ca794306699ba",
            "x86_64-unknown-linux-musl": "6f305888703691dd04cfff85284d23ea0b0146ed7c4415e472f1fb72b3f32cdf",
        },
        "strip_prefix": "python",
    },
    "3.12.9": {
        "url": "20250317/cpython-{python_version}+20250317-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "7c7fd9809da0382a601a79287b5d62d61ce0b15f5a5ee836233727a516e85381",
            "aarch64-unknown-linux-gnu": "00c6bf9acef21ac741fea24dc449d0149834d30e9113429e50a95cce4b00bb80",
            "ppc64le-unknown-linux-gnu": "25d77599dfd5849f17391d92da0da99079e4e94f19a881f763f5cc62530ef7e1",
            "riscv64-unknown-linux-gnu": "e97ab0fdf443b302c56a52b4fd08f513bf3be66aa47263f0f9df3c6e60e05f2e",
            "s390x-unknown-linux-gnu": "7492d079ffa8425c8f6c58e43b237c37e3fb7b31e2e14635927bb4d3397ba21e",
            "x86_64-apple-darwin": "1ee1b1bb9fbce5c145c4bec9a3c98d7a4fa22543e09a7c1d932bc8599283c2dc",
            "x86_64-pc-windows-msvc": "d15361fd202dd74ae9c3eece1abdab7655f1eba90bf6255cad1d7c53d463ed4d",
            "x86_64-unknown-linux-gnu": "ef382fb88cbb41a3b0801690bd716b8a1aec07a6c6471010bcc6bd14cd575226",
            "x86_64-unknown-linux-musl": "94e3837da1adf9964aab2d6047b33f70167de3096d1f9a2d1fa9340b1bbf537d",
        },
        "strip_prefix": "python",
    },
    "3.12.11": {
        "url": "20250808/cpython-{python_version}+20250808-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "8792c4a84c364ab975feca0c27d3157a5435b7baab325a346ae56b223893b661",
            "aarch64-unknown-linux-gnu": "4d7ba5314fab02130d6538f074961ffbf61310cade9180e59026074f9a8939cb",
            "aarch64-pc-windows-msvc": "00bf7d7e8bcf5d1e9c4dfca0247d8e035147777cd57ee9d4c64dedca86b0a464",
            "ppc64le-unknown-linux-gnu": "2c862eb40a81549d9c11e6bf5a7f07c3406310b14e6a4d16dcdf1c4763ef7090",
            "riscv64-unknown-linux-gnu": "0bb729b95fabd49c7b495f7c44a9086e3970ea57daf66365741574bd36a17e81",
            "s390x-unknown-linux-gnu": "99e465882d217d24ac90e99fac8f32e6a644d0340ac05ee510fb5cdf53f0cfb8",
            "x86_64-apple-darwin": "e0c932709dafb05f00e528a7560ef8ee559ac82b75faca60dd1245bca1c1553f",
            "x86_64-pc-windows-msvc": "81214ef71964a40ec269a79067ca490d45298c350583bc3af0e5781451a05c3c",
            "x86_64-unknown-linux-gnu": "63d78840bf209af8da8f24e335d910f88387b892ca9187be571d481c071751bb",
            "x86_64-unknown-linux-musl": "d633d070780590aa03ac5575cd9d7b9e17682d80f14b400313c009c387cf706b",
        },
        "strip_prefix": "python",
    },
    "3.12.12": {
        "url": "20251031/cpython-{python_version}+20251031-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "5e110cb821d2eb8246065d3b46faa655180c976c4e17250f7883c634a629bc63",
            "aarch64-unknown-linux-gnu": "81b644d166e0bfb918615af8a2363f8fcf26eccdcc60a5334b6a62c088470bac",
            "aarch64-pc-windows-msvc": "b190fed7c2b0f6e1010f554a0d1fd191c0754c4c0718e69d9d795ae559613780",
            "ppc64le-unknown-linux-gnu": "024f5e5678c9768d45cc24d37a8e9d265aae86c4a4602352dee3d7deba367052",
            "riscv64-unknown-linux-gnu": "b13c57fc372c131e667a99b9680f41c0b4da571cf99ed412103c2fe9ad5ed1fb",
            "s390x-unknown-linux-gnu": "2bf05bdd56cdf5ea4fd9f2faf151ea4211be96a0d1f4230b85f5dcae620d6400",
            "x86_64-apple-darwin": "687052a046d33be49dc95dd671816709067cf6176ed36c93ea61b1fe0b883b0f",
            "x86_64-pc-windows-msvc": "cff398b3f520c442a1b085dd347126c10c1b03f01ccc0decd8c897a687e893f1",
            "x86_64-unknown-linux-gnu": "80c3882f14e15cef8260ef5257d198e8f4371ca265887431d939e0d561de3253",
            "x86_64-unknown-linux-musl": "0a461330b9b89f2ea3088dde10d7a3f96aa65897b7c5ce2404fa3b5c4b8daa14",
        },
        "strip_prefix": "python",
    },
    "3.13.0": {
        "url": "20241016/cpython-{python_version}+20241016-{platform}-{build}.{ext}",
        "sha256": {
            "aarch64-apple-darwin": "31397953849d275aa2506580f3fa1cb5a85b6a3d392e495f8030e8b6412f5556",
            "aarch64-unknown-linux-gnu": "e8378c0162b2e0e4cc1f62b29443a3305d116d09583304dbb0149fecaff6347b",
            "ppc64le-unknown-linux-gnu": "fc4b7f27c4e84c78f3c8e6c7f8e4023e4638d11f1b36b6b5ce457b1926cebb53",
            "s390x-unknown-linux-gnu": "66b19e6a07717f6cfcd3a8ca953f0a2eaa232291142f3d26a8d17c979ec0f467",
            "x86_64-apple-darwin": "cff1b7e7cd26f2d47acac1ad6590e27d29829776f77e8afa067e9419f2f6ce77",
            "x86_64-pc-windows-msvc": "b25926e8ce4164cf103bacc4f4d154894ea53e07dd3fdd5ebb16fb1a82a7b1a0",
            "x86_64-unknown-linux-gnu": "2c8cb15c6a2caadaa98af51df6fe78a8155b8471cb3dd7b9836038e0d3657fb4",
            "x86_64-unknown-linux-musl": "2f61ee3b628a56aceea63b46c7afe2df3e22a61da706606b0c8efda57f953cf4",
            "aarch64-apple-darwin-freethreaded": "efc2e71c0e05bc5bedb7a846e05f28dd26491b1744ded35ed82f8b49ccfa684b",
            "aarch64-unknown-linux-gnu-freethreaded": "59b50df9826475d24bb7eff781fa3949112b5e9c92adb29e96a09cdf1216d5bd",
            "ppc64le-unknown-linux-gnu-freethreaded": "1217efa5f4ce67fcc9f7eb64165b1bd0912b2a21bc25c1a7e2cb174a21a5df7e",
            "s390x-unknown-linux-gnu-freethreaded": "6c3e1e4f19d2b018b65a7e3ef4cd4225c5b9adfbc490218628466e636d5c4b8c",
            "x86_64-apple-darwin-freethreaded": "2e07dfea62fe2215738551a179c87dbed1cc79d1b3654f4d7559889a6d5ce4eb",
            "x86_64-pc-windows-msvc-freethreaded": "bfd89f9acf866463bc4baf01733da5e767d13f5d0112175a4f57ba91f1541310",
            "x86_64-unknown-linux-gnu-freethreaded": "a73adeda301ad843cce05f31a2d3e76222b656984535a7b87696a24a098b216c",
        },
        "strip_prefix": {
            "aarch64-apple-darwin": "python",
            "aarch64-unknown-linux-gnu": "python",
            "ppc64le-unknown-linux-gnu": "python",
            "s390x-unknown-linux-gnu": "python",
            "x86_64-apple-darwin": "python",
            "x86_64-pc-windows-msvc": "python",
            "x86_64-unknown-linux-gnu": "python",
            "x86_64-unknown-linux-musl": "python",
            "aarch64-apple-darwin-freethreaded": "python/install",
            "aarch64-unknown-linux-gnu-freethreaded": "python/install",
            "ppc64le-unknown-linux-gnu-freethreaded": "python/install",
            "s390x-unknown-linux-gnu-freethreaded": "python/install",
            "x86_64-apple-darwin-freethreaded": "python/install",
            "x86_64-pc-windows-msvc-freethreaded": "python/install",
            "x86_64-unknown-linux-gnu-freethreaded": "python/install",
        },
    },
    "3.13.1": {
        "url": "20241205/cpython-{python_version}+20241205-{platform}-{build}.{ext}",
        "sha256": {
            "aarch64-apple-darwin": "88b88b609129c12f4b3841845aca13230f61e97ba97bd0fb28ee64b0e442a34f",
            "aarch64-unknown-linux-gnu": "fdfa86c2746d2ae700042c461846e6c37f70c249925b58de8cd02eb8d1423d4e",
            "ppc64le-unknown-linux-gnu": "27b20b3237c55430ca1304e687d021f88373f906249f9cd272c5ff2803d5e5c3",
            "s390x-unknown-linux-gnu": "7d0187e20cb5e36c689eec27e4d3de56d8b7f1c50dc5523550fc47377801521f",
            "x86_64-apple-darwin": "47eef6efb8664e2d1d23a7cdaf56262d784f8ace48f3bfca1b183e95a49888d6",
            "x86_64-pc-windows-msvc": "f51f0493a5f979ff0b8d8c598a8d74f2a4d86a190c2729c85e0af65c36a9cbbe",
            "x86_64-unknown-linux-gnu": "242b2727df6c1e00de6a9f0f0dcb4562e168d27f428c785b0eb41a6aeb34d69a",
            "x86_64-unknown-linux-musl": "76b30c6373b9c0aa2ba610e07da02f384aa210ac79643da38c66d3e6171c6ef5",
            "aarch64-apple-darwin-freethreaded": "08f05618bdcf8064a7960b25d9ba92155447c9b08e0cf2f46a981e4c6a1bb5a5",
            "aarch64-unknown-linux-gnu-freethreaded": "9f2fcb809f9ba6c7c014a8803073a88786701a98971135bce684355062e4bb35",
            "ppc64le-unknown-linux-gnu-freethreaded": "15ceea78dff78ca8ccaac8d9c54b808af30daaa126f1f561e920a6896e098634",
            "s390x-unknown-linux-gnu-freethreaded": "ed3c6118d1d12603309c930e93421ac7a30a69045ffd43006f63ecf71d72c317",
            "x86_64-apple-darwin-freethreaded": "dc780fecd215d2cc9e573abf1e13a175fcfa8f6efd100ef888494a248a16cda8",
            "x86_64-pc-windows-msvc-freethreaded": "7537b2ab361c0eabc0eabfca9ffd9862d7f5f6576eda13b97e98aceb5eea4fd3",
            "x86_64-unknown-linux-gnu-freethreaded": "9ec1b81213f849d91f5ebe6a16196e85cd6ff7c05ca823ce0ab7ba5b0e9fee84",
        },
        "strip_prefix": {
            "aarch64-apple-darwin": "python",
            "aarch64-unknown-linux-gnu": "python",
            "ppc64le-unknown-linux-gnu": "python",
            "s390x-unknown-linux-gnu": "python",
            "x86_64-apple-darwin": "python",
            "x86_64-pc-windows-msvc": "python",
            "x86_64-unknown-linux-gnu": "python",
            "x86_64-unknown-linux-musl": "python",
            "aarch64-apple-darwin-freethreaded": "python/install",
            "aarch64-unknown-linux-gnu-freethreaded": "python/install",
            "ppc64le-unknown-linux-gnu-freethreaded": "python/install",
            "s390x-unknown-linux-gnu-freethreaded": "python/install",
            "x86_64-apple-darwin-freethreaded": "python/install",
            "x86_64-pc-windows-msvc-freethreaded": "python/install",
            "x86_64-unknown-linux-gnu-freethreaded": "python/install",
        },
    },
    "3.13.2": {
        "url": "20250317/cpython-{python_version}+20250317-{platform}-{build}.{ext}",
        "sha256": {
            "aarch64-apple-darwin": "faa44274a331eb39786362818b21b3a4e74514e8805000b20b0e55c590cecb94",
            "aarch64-unknown-linux-gnu": "9c67260446fee6ea706dad577a0b32936c63f449c25d66e4383d5846b2ab2e36",
            "ppc64le-unknown-linux-gnu": "345b53d2f86c9dbd7f1320657cb227ff9a42ef63ff21f129abbbc8c82a375147",
            "riscv64-unknown-linux-gnu": "172d22b2330737f3a028ea538ffe497c39a066a8d3200b22dd4d177a3332ad85",
            "s390x-unknown-linux-gnu": "ec3b16ea8a97e3138acec72bc5ff35949950c62c8994a8ec8e213fd93f0e806b",
            "x86_64-apple-darwin": "ee4526e84b5ce5b11141c50060b385320f2773616249a741f90c96d460ce8e8f",
            "x86_64-pc-windows-msvc": "84d7b52f3558c8e35c670a4fa14080c75e3ec584adfae49fec8b51008b75b21e",
            "x86_64-unknown-linux-gnu": "db011f0cd29cab2291584958f4e2eb001b0e6051848d89b38a2dc23c5c54e512",
            "x86_64-unknown-linux-musl": "00bb2d629f7eacbb5c6b44dc04af26d1f1da64cee3425b0d8eb5135a93830296",
            "aarch64-apple-darwin-freethreaded": "c98c9c977e6fa05c3813bd49f3553904d89d60fed27e2e36468da7afa1d6d5e2",
            "aarch64-unknown-linux-gnu-freethreaded": "b8635e59e3143fd17f19a3dfe8ccc246ee6587c87da359bd1bcab35eefbb5f19",
            "ppc64le-unknown-linux-gnu-freethreaded": "6ae8fa44cb2edf4ab49cff1820b53c40c10349c0f39e11b8cd76ce7f3e7e1def",
            "riscv64-unknown-linux-gnu-freethreaded": "2af1b8850c52801fb6189e7a17a51e0c93d9e46ddefcca72247b76329c97d02a",
            "s390x-unknown-linux-gnu-freethreaded": "c074144cc80c2af32c420b79a9df26e8db405212619990c1fbdd308bd75afe3f",
            "x86_64-apple-darwin-freethreaded": "0d73e4348d8d4b5159058609d2303705190405b485dd09ad05d870d7e0f36e0f",
            "x86_64-pc-windows-msvc-freethreaded": "c51b4845fda5421e044067c111192f645234081d704313f74ee77fa013a186ea",
            "x86_64-unknown-linux-gnu-freethreaded": "1aea5062614c036904b55c1cc2fb4b500b7f6f7a4cacc263f4888889d355eef8",
        },
        "strip_prefix": {
            "aarch64-apple-darwin": "python",
            "aarch64-unknown-linux-gnu": "python",
            "ppc64le-unknown-linux-gnu": "python",
            "s390x-unknown-linux-gnu": "python",
            "riscv64-unknown-linux-gnu": "python",
            "x86_64-apple-darwin": "python",
            "x86_64-pc-windows-msvc": "python",
            "x86_64-unknown-linux-gnu": "python",
            "x86_64-unknown-linux-musl": "python",
            "aarch64-apple-darwin-freethreaded": "python/install",
            "aarch64-unknown-linux-gnu-freethreaded": "python/install",
            "ppc64le-unknown-linux-gnu-freethreaded": "python/install",
            "riscv64-unknown-linux-gnu-freethreaded": "python/install",
            "s390x-unknown-linux-gnu-freethreaded": "python/install",
            "x86_64-apple-darwin-freethreaded": "python/install",
            "x86_64-pc-windows-msvc-freethreaded": "python/install",
            "x86_64-unknown-linux-gnu-freethreaded": "python/install",
        },
    },
    "3.13.4": {
        "url": "20250610/cpython-{python_version}+20250610-{platform}-{build}.{ext}",
        "sha256": {
            "aarch64-apple-darwin": "c2ce6601b2668c7bd1f799986af5ddfbff36e88795741864aba6e578cb02ed7f",
            "aarch64-unknown-linux-gnu": "3c2596ece08ffe17e11bc1f27aeb4ce1195d2490a83d695d36ef4933d5c5ca53",
            "ppc64le-unknown-linux-gnu": "b3cc13ee177b8db1d3e9b2eac413484e3c6a356f97d91dc59de8d3fd8cf79d6b",
            "riscv64-unknown-linux-gnu": "d1b989e57a9ce29f6c945eeffe0e9750c222fdd09e99d2f8d6b0d8532a523053",
            "s390x-unknown-linux-gnu": "d1d19fb01961ac6476712fdd6c5031f74c83666f6f11aa066207e9a158f7e3d8",
            "x86_64-apple-darwin": "79feb6ca68f3921d07af52d9db06cf134e6f36916941ea850ab0bc20f5ff638b",
            "x86_64-pc-windows-msvc": "29ac3585cc2dcfd79e3fe380c272d00e9d34351fc456e149403c86d3fea34057",
            "x86_64-unknown-linux-gnu": "44e5477333ebca298a7a0a316985c6c3533b8645f92a83f7f73c44033832bf32",
            "x86_64-unknown-linux-musl": "a3afbfa94b9ff4d9fc426b47eb3c8446cada535075b8d51b7bdc9d9ab9911fc2",
            "aarch64-apple-darwin-freethreaded": "278dccade56b4bbeecb9a613b77012cf5c1433a5e9b8ef99230d5e61f31d9e02",
            "aarch64-unknown-linux-gnu-freethreaded": "b1c1bd6ab9ef95b464d92a6a911cef1a8d9f0b0f6a192f694ef18ed15d882edf",
            "ppc64le-unknown-linux-gnu-freethreaded": "ed66ae213a62b286b9b7338b816ccd2815f5248b7a28a185dc8159fe004149ae",
            "riscv64-unknown-linux-gnu-freethreaded": "913264545215236660e4178bc3e5b57a20a444a8deb5c11680c95afc960b4016",
            "s390x-unknown-linux-gnu-freethreaded": "7556a38ab5e507c1ec22bc38f9859982bc956cab7f4de05a2faac114feb306db",
            "x86_64-apple-darwin-freethreaded": "64ab7ac8c88002d9ba20a92f72945bfa350268e944a7922500af75d20330574d",
            "x86_64-pc-windows-msvc-freethreaded": "9457504547edb2e0156bf76b53c7e4941c7f61c0eff9fd5f4d816d3df51c58e3",
            "x86_64-unknown-linux-gnu-freethreaded": "864df6e6819e8f8e855ce30f34410fdc5867d0616e904daeb9a40e5806e970d7",
        },
        "strip_prefix": {
            "aarch64-apple-darwin": "python",
            "aarch64-unknown-linux-gnu": "python",
            "ppc64le-unknown-linux-gnu": "python",
            "s390x-unknown-linux-gnu": "python",
            "riscv64-unknown-linux-gnu": "python",
            "x86_64-apple-darwin": "python",
            "x86_64-pc-windows-msvc": "python",
            "x86_64-unknown-linux-gnu": "python",
            "x86_64-unknown-linux-musl": "python",
            "aarch64-apple-darwin-freethreaded": "python/install",
            "aarch64-unknown-linux-gnu-freethreaded": "python/install",
            "ppc64le-unknown-linux-gnu-freethreaded": "python/install",
            "riscv64-unknown-linux-gnu-freethreaded": "python/install",
            "s390x-unknown-linux-gnu-freethreaded": "python/install",
            "x86_64-apple-darwin-freethreaded": "python/install",
            "x86_64-pc-windows-msvc-freethreaded": "python/install",
            "x86_64-unknown-linux-gnu-freethreaded": "python/install",
        },
    },
    "3.13.6": {
        "url": "20250808/cpython-{python_version}+20250808-{platform}-{build}.{ext}",
        "sha256": {
            "aarch64-apple-darwin": "8a1efa6af4e80f08e2c97dda822a3d6c24d6c98e518242f802c6a43ae8401488",
            "aarch64-unknown-linux-gnu": "11fa0591ae2211c08a42ae54944260e36ddf88a1d5604ea0c49e2477be4e5388",
            "ppc64le-unknown-linux-gnu": "8dcf34ae1a685fe1893b52917ae04f23328edadc4acae28499d43850c2bdd26c",
            "riscv64-unknown-linux-gnu": "f8ed75aa6cc2011a046be00b629c3c8295267f34280324feaff34c73e7afce39",
            "s390x-unknown-linux-gnu": "7707ee5d19a78bc64ef8a66751ec7f97b64ea06714c7b1b52e8b321c2923ead8",
            "x86_64-apple-darwin": "27badce7201321a8363219e438a6205165e5b4884012b1046532203df2ec9379",
            "x86_64-pc-windows-msvc": "af5cc733c33b9aa9f1d74c81a59351e9b27215486d8b6cdbc06d97646a58c953",
            "aarch64-pc-windows-msvc": "8e1617bd407ec1a874499daab26ae95080d1e0267ae616d34490137a28705827",
            "aarch64-pc-windows-msvc-freethreaded": "552cfabcc3b103f4b1c4036d2592d5f0373c9554a2c4d2b6631b04ef7e592067",
            "x86_64-unknown-linux-gnu": "f844e8c8b6847628b472f7e97d8893a4e93acd5382a902b465776063668c4d64",
            "x86_64-unknown-linux-musl": "70076dea0ff65b3c05aae1a97b4a556bf613cc73db30309e59134f9d318f4f7b",
            "aarch64-apple-darwin-freethreaded": "f2143304012e021a603bf1807bf3e4ce163832e43ab9a9829e53cb136497f207",
            "aarch64-unknown-linux-gnu-freethreaded": "d84a7d64c284be387386b9f5da273f6d05486eb6bd8f9e86e2575cb59604cb22",
            "ppc64le-unknown-linux-gnu-freethreaded": "e76fcaf1bf80a615520dbe7f85ca0bb557fad96d132d836b0ac721e7cc1e2a37",
            "riscv64-unknown-linux-gnu-freethreaded": "24e08a39ba4fc77753e61541e52eed39cc871f4a92a80a3c5dd495056bd8eff9",
            "s390x-unknown-linux-gnu-freethreaded": "1609b223fd38a4a7a4d20e7173d7d9390fe2258f7dd9a15dc9ef0fa49613735d",
            "x86_64-apple-darwin-freethreaded": "4360a1278dd0a96b526d108c8fd23498a9d2028dd7791e510fd51ff5ea3f462a",
            "x86_64-pc-windows-msvc-freethreaded": "4e727cdbe4057b16a170f887c0fa4227a825ac59bcda84ae946c77cc932af78c",
            "x86_64-unknown-linux-gnu-freethreaded": "e48c13c59cc3c01b79f63c8bccec27d2db6e97f64213b8731e2077b6ed8ed52c",
        },
        "strip_prefix": {
            "aarch64-apple-darwin": "python",
            "aarch64-unknown-linux-gnu": "python",
            "ppc64le-unknown-linux-gnu": "python",
            "s390x-unknown-linux-gnu": "python",
            "riscv64-unknown-linux-gnu": "python",
            "x86_64-apple-darwin": "python",
            "x86_64-pc-windows-msvc": "python",
            "aarch64-pc-windows-msvc": "python",
            "x86_64-unknown-linux-gnu": "python",
            "x86_64-unknown-linux-musl": "python",
            "aarch64-apple-darwin-freethreaded": "python/install",
            "aarch64-unknown-linux-gnu-freethreaded": "python/install",
            "ppc64le-unknown-linux-gnu-freethreaded": "python/install",
            "riscv64-unknown-linux-gnu-freethreaded": "python/install",
            "s390x-unknown-linux-gnu-freethreaded": "python/install",
            "x86_64-apple-darwin-freethreaded": "python/install",
            "x86_64-pc-windows-msvc-freethreaded": "python/install",
            "aarch64-pc-windows-msvc-freethreaded": "python/install",
            "x86_64-unknown-linux-gnu-freethreaded": "python/install",
        },
    },
    "3.13.9": {
        "url": "20251031/cpython-{python_version}+20251031-{platform}-{build}.{ext}",
        "sha256": {
            "aarch64-apple-darwin": "1f3568d17383426d52350c2ef7c93c1a5a043198b860cb05e5d19b35f9c25cef",
            "aarch64-unknown-linux-gnu": "0a56d11b0fb1662e67f892b9d5d1717aef06f24dbb8362bc25b8f784e620d44e",
            "ppc64le-unknown-linux-gnu": "99492123902bd5e9a6b1a30135061e93a2e6a11d25107a741d5a756e91054448",
            "riscv64-unknown-linux-gnu": "b3dce3e4ef508773521e1ee1be989fff6118f8fd1fbbd0491d7ff7dfbc98ef06",
            "s390x-unknown-linux-gnu": "f10e34aaa856c1b8a69c2ea4a9a6723d520443d1a957bf66dc55491334ca0c1e",
            "x86_64-apple-darwin": "48c0f3ca5d31e90658ef99138dc21865bb62f388ab97a1ce72cac176da194ab0",
            "x86_64-pc-windows-msvc": "874593f641f31ea101440c70f81768c35d4d7d6df111fde63094db67465ef787",
            "aarch64-pc-windows-msvc": "20db43873d3c4c2175d866806545e4ad4ec6bb72ca95e60082a4df6c24567e8c",
            "aarch64-pc-windows-msvc-freethreaded": "743ff69935ef28834621647dab30f032dfcd80315732917531eea333210941c7",
            "x86_64-unknown-linux-gnu": "6f05b91ee8c7e6dd0f9c60b95bb29130e2d623961de6578b643e80ddd83f96b6",
            "x86_64-unknown-linux-musl": "ad987197034185e628715da504a50613af213dc21ba6d5ccaeab3db2c464aa6c",
            "aarch64-apple-darwin-freethreaded": "eae1272a72ccce601590a10a9ca2a58199b5fcdf022aa603a527e3e2a04de9bc",
            "aarch64-unknown-linux-gnu-freethreaded": "a6e72f9de5d9b46cf6968d6a492f2401a919f9b959f8da2d87f43484b80169ee",
            "ppc64le-unknown-linux-gnu-freethreaded": "0ed5c65437f875c58ba1bee2b8d261d18698d3d0347a2e66f8902fce022a2cda",
            "riscv64-unknown-linux-gnu-freethreaded": "584e481d9b5225ffaf02f158fb26d2818207e65fc3c6dc21a6d500277f739220",
            "s390x-unknown-linux-gnu-freethreaded": "7fa7fb912ca989ceac026a332d56a2c7d6d16ab0e94d89e690de5aade26103e2",
            "x86_64-apple-darwin-freethreaded": "e2bf5fa6a3ef443ade362e08b0a19bbc172f7bfe34dabe933ccaad31d53af5da",
            "x86_64-pc-windows-msvc-freethreaded": "318a9a1e43dd52054327de3bccc0c5b7afde7b7f2a398ccb4d38e03d28b05386",
            "x86_64-unknown-linux-gnu-freethreaded": "dcc29b069d0588fbd4ea29c6df840c8d1207d2a3bce8cd5cd57d1b85373b6048",
        },
        "strip_prefix": {
            "aarch64-apple-darwin": "python",
            "aarch64-unknown-linux-gnu": "python",
            "ppc64le-unknown-linux-gnu": "python",
            "s390x-unknown-linux-gnu": "python",
            "riscv64-unknown-linux-gnu": "python",
            "x86_64-apple-darwin": "python",
            "x86_64-pc-windows-msvc": "python",
            "aarch64-pc-windows-msvc": "python",
            "x86_64-unknown-linux-gnu": "python",
            "x86_64-unknown-linux-musl": "python",
            "aarch64-apple-darwin-freethreaded": "python/install",
            "aarch64-unknown-linux-gnu-freethreaded": "python/install",
            "ppc64le-unknown-linux-gnu-freethreaded": "python/install",
            "riscv64-unknown-linux-gnu-freethreaded": "python/install",
            "s390x-unknown-linux-gnu-freethreaded": "python/install",
            "x86_64-apple-darwin-freethreaded": "python/install",
            "x86_64-pc-windows-msvc-freethreaded": "python/install",
            "aarch64-pc-windows-msvc-freethreaded": "python/install",
            "x86_64-unknown-linux-gnu-freethreaded": "python/install",
        },
    },
    "3.13.10": {
        "url": "20251202/cpython-{python_version}+20251202-{platform}-{build}.{ext}",
        "sha256": {
            "aarch64-apple-darwin": "37afe4e77ab62ac50f197b1cb1f3bc02c82735c6be893da0996afcde5dc41048",
            "aarch64-unknown-linux-gnu": "c68280591cda1c9515a04809fa6926020177e8e5892300206e0496ea1d10290e",
            "ppc64le-unknown-linux-gnu": "1507e5528bd88131dc742a2941176aceea1838bc09860c21f179285b7865133b",
            "riscv64-unknown-linux-gnu": "70169e916860b2e5b34c37c302d699eb2b8f24f28090968881942a37aeb7ed08",
            "s390x-unknown-linux-gnu": "c5448863b64aacae62f3a213a6e6cf94ec63f96ee4d518491cd62fd3c81d952f",
            "x86_64-apple-darwin": "a02761a4f189f71c0512e88df7ca2843696d61da659e47f8a5c8a9bd2c0d16f4",
            "x86_64-pc-windows-msvc": "8b00014c7c35f9ad4cb1c565f067500bacc4125c8bc30e4389ee0be9fd6ffa3d",
            "aarch64-pc-windows-msvc": "9060d644bd32ac0e0af970d0b21e207e6ff416b7c4dc26ffc4f9b043fb45b463",
            "aarch64-pc-windows-msvc-freethreaded": "cdb7141327bdc244715b25752593e2c9eeb3cc2764f37dfe81cfbc92db9d6d57",
            "x86_64-unknown-linux-gnu": "0cac1495fff920219904b1d573aaec0df54d549c226cb45f5c60cb6d2c72727a",
            "x86_64-unknown-linux-musl": "04108190972ac98e13098abd972ec3f4f8b0880f83c0bb68249ce1a6164fa041",
            "aarch64-apple-darwin-freethreaded": "3c9fdd76447c1549a0d3bc2a70c63f1daec997ab034206ac0260a03237166dbb",
            "aarch64-unknown-linux-gnu-freethreaded": "6d277221fa4b172e00b29c7158ca9661917bc8db9a0084b1a0ff5c3a0ba8b648",
            "ppc64le-unknown-linux-gnu-freethreaded": "d265d8d1c51e25ed70279540223589f79cf99ad00b50d28b6150c2658c973885",
            "riscv64-unknown-linux-gnu-freethreaded": "ec411b4a2d167c3be0a9aeb3905e045d62c8e3c3db0caeade5d47d5f60b98dd0",
            "s390x-unknown-linux-gnu-freethreaded": "4fc6443948bf5b729481ea02cc5c68e80cd0da42631f6936587a2b8fd45bc62c",
            "x86_64-apple-darwin-freethreaded": "6ce608684df0f90350c7a1742e9685a7782d9b26ec99d1bd9d55c8cf9a405040",
            "x86_64-pc-windows-msvc-freethreaded": "6a8b0372ded655e0d55318089fbce3122a446e69bcd120c79aaadfe9b017299c",
            "x86_64-unknown-linux-gnu-freethreaded": "e39127fbe8d2ae7d86099f18b4da0918f9b60ce73ed491774d6dcfaa42b5c9ae",
        },
        "strip_prefix": {
            "aarch64-apple-darwin": "python",
            "aarch64-unknown-linux-gnu": "python",
            "ppc64le-unknown-linux-gnu": "python",
            "s390x-unknown-linux-gnu": "python",
            "riscv64-unknown-linux-gnu": "python",
            "x86_64-apple-darwin": "python",
            "x86_64-pc-windows-msvc": "python",
            "aarch64-pc-windows-msvc": "python",
            "x86_64-unknown-linux-gnu": "python",
            "x86_64-unknown-linux-musl": "python",
            "aarch64-apple-darwin-freethreaded": "python/install",
            "aarch64-unknown-linux-gnu-freethreaded": "python/install",
            "ppc64le-unknown-linux-gnu-freethreaded": "python/install",
            "riscv64-unknown-linux-gnu-freethreaded": "python/install",
            "s390x-unknown-linux-gnu-freethreaded": "python/install",
            "x86_64-apple-darwin-freethreaded": "python/install",
            "x86_64-pc-windows-msvc-freethreaded": "python/install",
            "aarch64-pc-windows-msvc-freethreaded": "python/install",
            "x86_64-unknown-linux-gnu-freethreaded": "python/install",
        },
    },
    "3.13.11": {
        "url": "20251209/cpython-{python_version}+20251209-{platform}-{build}.{ext}",
        "sha256": {
            "aarch64-apple-darwin": "295a9f7bc899ea1cc08baf60bbf511bdd1e4a29b2dd7e5f59b48f18bfa6bf585",
            "aarch64-unknown-linux-gnu": "ea1e678e6e82301bb32bf3917732125949b6e46d541504465972024a3f165343",
            "ppc64le-unknown-linux-gnu": "7660e53aad9d35ee256913c6d98427f81f078699962035c5fa8b5c3138695109",
            "riscv64-unknown-linux-gnu": "763fa1548e6a432e9402916e690c74ea30f26dcd2e131893dd506f72b87c27c9",
            "s390x-unknown-linux-gnu": "ffb6af51fbfabfc6fbc4e7379bdec70c2f51e972b1d2f45c053493b9da3a1bbe",
            "x86_64-apple-darwin": "dac4a0a0a9b71f6b02a8b0886547fa22814474239bffb948e3e77185406ea136",
            "x86_64-pc-windows-msvc": "87822417007045a28a7eccc47fe67b8c61265b99b10dbbfa24d231a3622b1c27",
            "aarch64-pc-windows-msvc": "ba646d0c3b7dd7bdfb770d9b2ebd6cd2df02a37fda90c9c79a7cf59c7df6f165",
            "aarch64-pc-windows-msvc-freethreaded": "6daf6d092c7294cfe68c4c7bf2698ac134235489c874b3bf796c7972b9dbba30",
            "x86_64-unknown-linux-gnu": "1ffa06d714a44aea14c0c54c30656413e5955a6c92074b4b3cb4351dcc28b63b",
            "x86_64-unknown-linux-musl": "969fe24017380b987c4e3ce15e9edf82a4618c1e61672b2cc9b021a1c98eae78",
            "aarch64-apple-darwin-freethreaded": "4213058b7fcd875596c12b58cd46a399358b0a87ecde4b349cbdd00cf87ed79a",
            "aarch64-unknown-linux-gnu-freethreaded": "290ca3bd0007db9e551f90b08dfcb6c1b2d62c33b2fc3e9a43e77d385d94f569",
            "ppc64le-unknown-linux-gnu-freethreaded": "09d4b50f8abb443f7e3af858c920aa61c2430b0954df465e861caa7078e55e69",
            "riscv64-unknown-linux-gnu-freethreaded": "5406f2a7cacafbd2aac3ce2de066a0929aab55423824276c36e04cb83babc36c",
            "s390x-unknown-linux-gnu-freethreaded": "3984b67c4292892eaccdd1c094c7ec788884c4c9b3534ab6995f6be96d5ed51d",
            "x86_64-apple-darwin-freethreaded": "d6f489464045d6895ae68b0a04a9e16477e74fe3185a75f3a9a0af8ccd25eade",
            "x86_64-pc-windows-msvc-freethreaded": "bb9a29a7ba8f179273b79971da6aaa7be592d78c606a63f99eff3e4c12fb0fae",
            "x86_64-unknown-linux-gnu-freethreaded": "33f89c957d986d525529b8a980103735776f4d20cf52f55960a057c760188ac3",
        },
        "strip_prefix": {
            "aarch64-apple-darwin": "python",
            "aarch64-unknown-linux-gnu": "python",
            "ppc64le-unknown-linux-gnu": "python",
            "s390x-unknown-linux-gnu": "python",
            "riscv64-unknown-linux-gnu": "python",
            "x86_64-apple-darwin": "python",
            "x86_64-pc-windows-msvc": "python",
            "aarch64-pc-windows-msvc": "python",
            "x86_64-unknown-linux-gnu": "python",
            "x86_64-unknown-linux-musl": "python",
            "aarch64-apple-darwin-freethreaded": "python/install",
            "aarch64-unknown-linux-gnu-freethreaded": "python/install",
            "ppc64le-unknown-linux-gnu-freethreaded": "python/install",
            "riscv64-unknown-linux-gnu-freethreaded": "python/install",
            "s390x-unknown-linux-gnu-freethreaded": "python/install",
            "x86_64-apple-darwin-freethreaded": "python/install",
            "x86_64-pc-windows-msvc-freethreaded": "python/install",
            "aarch64-pc-windows-msvc-freethreaded": "python/install",
            "x86_64-unknown-linux-gnu-freethreaded": "python/install",
        },
    },
    "3.14.0": {
        "url": "20251031/cpython-{python_version}+20251031-{platform}-{build}.{ext}",
        "sha256": {
            "aarch64-apple-darwin": "b4bcd3c6c24cab32ae99e1b05c89312b783b4d69431d702e5012fe1fdcad4087",
            "aarch64-unknown-linux-gnu": "128a9cbfb9645d5237ec01704d9d1d2ac5f084464cc43c37a4cd96aa9c3b1ad5",
            "ppc64le-unknown-linux-gnu": "e16ca51f018e99a609faf953bd3a3aea31f45ee84262d1a517fb3abd98f1f4af",
            "riscv64-unknown-linux-gnu": "fca340d8fb7a05cd90e216ce601b25d492ed8c1a3b6a6d77703e0f15ab3711a7",
            "s390x-unknown-linux-gnu": "c5803644970eee931bb0581b3b64511d1a8612f67bc98951a7f7ab5581a9ed04",
            "x86_64-apple-darwin": "4e71a3ce973be377ef18637826648bb936e2f9490f64a9e4f33a49bcc431d344",
            "x86_64-pc-windows-msvc": "39acfcb3857d83eab054a3de11756ffc16b3d49c31393b9800dd2704d1f07fdf",
            "aarch64-pc-windows-msvc": "599a8b7e12439cd95a201dbdfe95cf363146b1ff91f379555dafd86b170caab9",
            "x86_64-unknown-linux-gnu": "3dec1ab70758a3467ac3313bbcdabf7a9b3016db5c072c4537e3cf0a9e6290f6",
            "x86_64-unknown-linux-musl": "d0a2a6d3b1bb00dce2105377fda8aa79675d187f8d6d7010a42f651af25018dc",
            "aarch64-apple-darwin-freethreaded": "d9c7b430b25bd3837dbb03f945dbe6b7bc526c5940ca96f5db7cdc42f6b2b801",
            "aarch64-unknown-linux-gnu-freethreaded": "f383ef50d1da6ca511212e5ae601923b56636b87351fd5fc847e0ea0a19fa9b3",
            "ppc64le-unknown-linux-gnu-freethreaded": "cb0e4ff781b856a47f0f461ceb41c78c7eeff65effd0957857ec4702ef1e1bd3",
            "riscv64-unknown-linux-gnu-freethreaded": "929223470d11a55cd75f880ac3bd4969e42407e2cdf08d4e7e38ba721cf4abec",
            "s390x-unknown-linux-gnu-freethreaded": "613fb1f7b249f798b52af957d181305244e936c8e5c94c84688fcdf93fe14253",
            "x86_64-apple-darwin-freethreaded": "b3196f6b57bbb3dc2ee07f348f1d51117ffa376979eceafbf50c15f0f7980bf8",
            "x86_64-pc-windows-msvc-freethreaded": "b81de5fc9e783ea6dfcf1098c28a278c874999c71afbb0309f6a8b4276c769d0",
            "aarch64-pc-windows-msvc-freethreaded": "40266e60f655e49cd1d5303295255909a4b593b08b88be6e6a55b2c9fe6ed13d",
            "x86_64-unknown-linux-gnu-freethreaded": "f4acbef0fbfaf7ab31ac63986da1d93dfa1c5cb797de1dcdc1a988aa18670120",
        },
        "strip_prefix": {
            "aarch64-apple-darwin": "python",
            "aarch64-unknown-linux-gnu": "python",
            "ppc64le-unknown-linux-gnu": "python",
            "s390x-unknown-linux-gnu": "python",
            "riscv64-unknown-linux-gnu": "python",
            "x86_64-apple-darwin": "python",
            "x86_64-pc-windows-msvc": "python",
            "aarch64-pc-windows-msvc": "python",
            "x86_64-unknown-linux-gnu": "python",
            "x86_64-unknown-linux-musl": "python",
            "aarch64-apple-darwin-freethreaded": "python/install",
            "aarch64-unknown-linux-gnu-freethreaded": "python/install",
            "ppc64le-unknown-linux-gnu-freethreaded": "python/install",
            "riscv64-unknown-linux-gnu-freethreaded": "python/install",
            "s390x-unknown-linux-gnu-freethreaded": "python/install",
            "x86_64-apple-darwin-freethreaded": "python/install",
            "x86_64-pc-windows-msvc-freethreaded": "python/install",
            "aarch64-pc-windows-msvc-freethreaded": "python/install",
            "x86_64-unknown-linux-gnu-freethreaded": "python/install",
        },
    },
    "3.14.1": {
        "url": "20251202/cpython-{python_version}+20251202-{platform}-{build}.{ext}",
        "sha256": {
            "aarch64-apple-darwin": "cdf1ba0789f529fa34bb5b5619c5da9757ac1067d6b8dd0ee8b78e50078fc561",
            "aarch64-unknown-linux-gnu": "5dde7dba0b8ef34c0d5cb8a721254b1e11028bfc09ff06664879c245fe8df73f",
            "ppc64le-unknown-linux-gnu": "d2774701d53e2ac06f8c8c8e52dfa4ff346890de9b417c9a7664195443a4c766",
            "riscv64-unknown-linux-gnu": "af840506efbcd5026d9140c0a0230e45e46bb1f339a65c10a22875930b2c0159",
            "s390x-unknown-linux-gnu": "43f8f79bf4c66689d2019f193671d1df3e5e5dbb293382036285e8ce55fc55bb",
            "x86_64-apple-darwin": "f25ce050e1d370f9c05c9623b769ffa4b269a6ae17e611b435fd2b8b09972a88",
            "x86_64-pc-windows-msvc": "cb478a5a37eb93ce4d3c27ae64d211d6a5a42475ae53f666a8d1570e71fcf409",
            "aarch64-pc-windows-msvc": "19129cf8b4d68c4e64c25bae43bca139d871267b59cf7f02b9dcf25f0bf59497",
            "x86_64-unknown-linux-gnu": "a72f313bad49846e5e9671af2be7476033a877c80831cf47f431400ccb520090",
            "x86_64-unknown-linux-musl": "15d50b15713097c38c67b1a06a0498ad102377f9b3999e98e4eefd6bf91bd82d",
            "aarch64-apple-darwin-freethreaded": "61f38e947449cf00f32f0838e813358f6bf61025d0797531e5b8b8b175c617f0",
            "aarch64-unknown-linux-gnu-freethreaded": "1a88a1fe21eb443d280999464b1a397605a7ca950d8ab73813ca6868835439a2",
            "ppc64le-unknown-linux-gnu-freethreaded": "7207b736ed2569f307649ffd4b615a5346631bc244730b8702babee377cef528",
            "riscv64-unknown-linux-gnu-freethreaded": "d1356ccd279920edc31bf0350674d966beb9522f9503846ed7855dbb109ccc14",
            "s390x-unknown-linux-gnu-freethreaded": "477758eabc06dbc7e5e5d16e97c4672478acd409f420dd2e1b84d3452c0668d1",
            "x86_64-apple-darwin-freethreaded": "c2cb2a9b44285fbc13c3c9b7eea813db6ed8d94909406b059db7afd39b32e786",
            "x86_64-pc-windows-msvc-freethreaded": "8ef7048315cac6d26bdbef18512a87b1a24fffa21cec86e32f9a9425f2af9bf6",
            "aarch64-pc-windows-msvc-freethreaded": "ddb10b645de2b1f6f2832a80b115a9cd34a4a760249983027efe46618a8efc48",
            "x86_64-unknown-linux-gnu-freethreaded": "c5d5b89aab7de683e465e36de2477a131435076badda775ef6e9ea21109c1c32",
        },
        "strip_prefix": {
            "aarch64-apple-darwin": "python",
            "aarch64-unknown-linux-gnu": "python",
            "ppc64le-unknown-linux-gnu": "python",
            "s390x-unknown-linux-gnu": "python",
            "riscv64-unknown-linux-gnu": "python",
            "x86_64-apple-darwin": "python",
            "x86_64-pc-windows-msvc": "python",
            "aarch64-pc-windows-msvc": "python",
            "x86_64-unknown-linux-gnu": "python",
            "x86_64-unknown-linux-musl": "python",
            "aarch64-apple-darwin-freethreaded": "python/install",
            "aarch64-unknown-linux-gnu-freethreaded": "python/install",
            "ppc64le-unknown-linux-gnu-freethreaded": "python/install",
            "riscv64-unknown-linux-gnu-freethreaded": "python/install",
            "s390x-unknown-linux-gnu-freethreaded": "python/install",
            "x86_64-apple-darwin-freethreaded": "python/install",
            "x86_64-pc-windows-msvc-freethreaded": "python/install",
            "aarch64-pc-windows-msvc-freethreaded": "python/install",
            "x86_64-unknown-linux-gnu-freethreaded": "python/install",
        },
    },
    "3.14.2": {
        "url": "20251209/cpython-{python_version}+20251209-{platform}-{build}.{ext}",
        "sha256": {
            "aarch64-apple-darwin": "2f74bd26bd16487aca357c879d11f7b16c0521328e5148a1930ab6357bcb89fe",
            "aarch64-unknown-linux-gnu": "869af31b2963194e8a2ecfadc36027c4c1c86a10f4960baec36dadb41b2acf02",
            "ppc64le-unknown-linux-gnu": "86129976403fb5d64cf576329f94148f28cf6f82834e94df81ff31e9d5f404e0",
            "riscv64-unknown-linux-gnu": "318dceecf119ea903aef1fb03a552cc592ecd61c08da891b68f5755e21e13511",
            "s390x-unknown-linux-gnu": "53875c849a14194344ead1d9cd1e128cadd42a4b83c35eeb212417909ef05a6a",
            "x86_64-apple-darwin": "58fa3e17d13ab956fd11055fb774c98ecfddcdf3b588e5f2369bdbc14ef9d76a",
            "x86_64-pc-windows-msvc": "0d660bba9f58cb552e7e99e1f96a9c67b41618c9b8d29f9f3515fe2b5ad1966e",
            "aarch64-pc-windows-msvc": "0be0d2557d73efa7f6f3f99679f05252d57fe2aad2d81cac3cad410a9b1eacbd",
            "x86_64-unknown-linux-gnu": "121c3249bef497adf601df76a4d89aed6053fc5ec2f8c0ec656b86f0142e8ddd",
            "x86_64-unknown-linux-musl": "71639cc5d1fb79840467531c5b53ca77170a58edd3f7e2d29330dd736e477469",
            "aarch64-apple-darwin-freethreaded": "d6d17b8ef28326552cdeb2a7541c8a0cb711b378df9b93ebdb461dca065edfea",
            "aarch64-unknown-linux-gnu-freethreaded": "adfcb90f3a7e1b3fbc6a99f9c8c8dce1f2e26ea72b724bbe4e9fa39e81e2b0db",
            "ppc64le-unknown-linux-gnu-freethreaded": "2b1ce0c5a5f5e5add7e4f934f5bd35ac41660895a30b3098db7f7303d6952a4f",
            "riscv64-unknown-linux-gnu-freethreaded": "4efb610fa07a6ee2639d14d78fc3b6ecb47431c14e1e4bda03c7f7dd60a5c1e5",
            "s390x-unknown-linux-gnu-freethreaded": "e62f3bb3e66dac6c459690f9e9cd8cc2f6fe1dcf8bfed452af4c3df24cd7874f",
            "x86_64-apple-darwin-freethreaded": "1fd76c79f7fc1753e8d2ed2f71406c0b65776c75f3e95ed99ffde8c95af2adc1",
            "x86_64-pc-windows-msvc-freethreaded": "9927951e3997c186d2813ca1a0f4a8f5a2f771463f7f8ad0752fd3d2be2b74e4",
            "aarch64-pc-windows-msvc-freethreaded": "43aac5bb4cdba71fc6775d26f47348d573a0b1210911438be71d7d96f4b18b51",
            "x86_64-unknown-linux-gnu-freethreaded": "3728872ffd74989a7b4bbf3f0c629ae8fe821cda2bd6544012c1b92b9f5d5a5b",
        },
        "strip_prefix": {
            "aarch64-apple-darwin": "python",
            "aarch64-unknown-linux-gnu": "python",
            "ppc64le-unknown-linux-gnu": "python",
            "s390x-unknown-linux-gnu": "python",
            "riscv64-unknown-linux-gnu": "python",
            "x86_64-apple-darwin": "python",
            "x86_64-pc-windows-msvc": "python",
            "aarch64-pc-windows-msvc": "python",
            "x86_64-unknown-linux-gnu": "python",
            "x86_64-unknown-linux-musl": "python",
            "aarch64-apple-darwin-freethreaded": "python/install",
            "aarch64-unknown-linux-gnu-freethreaded": "python/install",
            "ppc64le-unknown-linux-gnu-freethreaded": "python/install",
            "riscv64-unknown-linux-gnu-freethreaded": "python/install",
            "s390x-unknown-linux-gnu-freethreaded": "python/install",
            "x86_64-apple-darwin-freethreaded": "python/install",
            "x86_64-pc-windows-msvc-freethreaded": "python/install",
            "aarch64-pc-windows-msvc-freethreaded": "python/install",
            "x86_64-unknown-linux-gnu-freethreaded": "python/install",
        },
    },
    "3.15.0a1": {
        "url": "20251031/cpython-{python_version}+20251031-{platform}-{build}.{ext}",
        "sha256": {
            "aarch64-apple-darwin": "3acf7aa3559b746498b18929456c5cacb84bae4e09249834cbc818970d71de87",
            "aarch64-unknown-linux-gnu": "d55c2aeece827e6bec83fd18515ee281d9ea0efaa3e2d20130db8f1c7cbb71c6",
            "ppc64le-unknown-linux-gnu": "c28beda791c499b16f06256339522f0002a3e9acba003e6b8374755d7be1def2",
            "riscv64-unknown-linux-gnu": "36619f576b8154e4b56643c5c4a85c352f152df2989c4e602cbbe9c2b7ded870",
            "s390x-unknown-linux-gnu": "5ea47be2a3a563ddd87ff510dae26b7aa7f3855ca00c5f1056ff8114c067c4e4",
            "x86_64-apple-darwin": "0ab19d3ac25f99da438b088751e5ec2421f9f6aa4292fd2dc0f8e49eb3e16bdf",
            "x86_64-pc-windows-msvc": "5f5d6bec2b381cfc771c49972d2a6f7b7e7ab6a1651d8fb6ef3983f3571722b3",
            "aarch64-pc-windows-msvc": "1508bcd7195008479ed156aad3afbb3a3793097ed530690f0304a8107f0e53e8",
            "x86_64-unknown-linux-gnu": "1f356288c2b2713619cb7a4e453d33bf8882f812af2987e21e01e7ae382fefba",
            "x86_64-unknown-linux-musl": "caf5311f333eef082dd69a669ca65aceba09a08fc1e78aad602ad649106f294c",
            "aarch64-apple-darwin-freethreaded": "12f1b16be4017181ad67904caf9e59e525b9b5d62f49105017d837e27b832959",
            "aarch64-unknown-linux-gnu-freethreaded": "981fe8dfc6e7e1d0ffefa945a18d5c4c759bbe21722acf3a5cc7e62f16aa5f3c",
            "ppc64le-unknown-linux-gnu-freethreaded": "088400dec25139f38eeecb48f090ff2ce06a96a1dd79fa8f1dfec1cd1786f5ef",
            "riscv64-unknown-linux-gnu-freethreaded": "938061a0a31a06672526885de36037ddefd8c4acdb09424691b7000a8c8f8d01",
            "s390x-unknown-linux-gnu-freethreaded": "2003e7e40bb44b3db7bca81087bfb738fe6af40e5db61cda8e23b59bf55d409e",
            "x86_64-apple-darwin-freethreaded": "64fc29e6c7a2f02a18645d968f1b3fc1d00d12a5ef3fcbb0d077fa8c62c08904",
            "x86_64-pc-windows-msvc-freethreaded": "34abc5603e1b4131f753d29b7deac865b9277912b851cbed5a149cf3e6745d3d",
            "aarch64-pc-windows-msvc-freethreaded": "54ca78dae455ece6fefbd7f5f287cc55d5ce197caf51921f6d871d15069d9489",
            "x86_64-unknown-linux-gnu-freethreaded": "0e0272186d9f5169394dbc4d4d72a3f4a5762a04c2e5ac2ab1e23aa41fc8538a",
        },
        "strip_prefix": {
            "aarch64-apple-darwin": "python",
            "aarch64-unknown-linux-gnu": "python",
            "ppc64le-unknown-linux-gnu": "python",
            "s390x-unknown-linux-gnu": "python",
            "riscv64-unknown-linux-gnu": "python",
            "x86_64-apple-darwin": "python",
            "x86_64-pc-windows-msvc": "python",
            "aarch64-pc-windows-msvc": "python",
            "x86_64-unknown-linux-gnu": "python",
            "x86_64-unknown-linux-musl": "python",
            "aarch64-apple-darwin-freethreaded": "python/install",
            "aarch64-unknown-linux-gnu-freethreaded": "python/install",
            "ppc64le-unknown-linux-gnu-freethreaded": "python/install",
            "riscv64-unknown-linux-gnu-freethreaded": "python/install",
            "s390x-unknown-linux-gnu-freethreaded": "python/install",
            "x86_64-apple-darwin-freethreaded": "python/install",
            "x86_64-pc-windows-msvc-freethreaded": "python/install",
            "aarch64-pc-windows-msvc-freethreaded": "python/install",
            "x86_64-unknown-linux-gnu-freethreaded": "python/install",
        },
    },
    "3.15.0a2": {
        "url": "20251209/cpython-{python_version}+20251209-{platform}-{build}.{ext}",
        "sha256": {
            "aarch64-apple-darwin": "5851f3744fbd39e3e323844cf4f68d7763fb25546aa5ffbb71b1b5ab69c56616",
            "aarch64-unknown-linux-gnu": "17ba65d669be3052524e03b4d1426c072ef38df2a9065ff4525d1f4d1bc9f82c",
            "ppc64le-unknown-linux-gnu": "5585bd7c5eefe28b9bf544d902cad9a2f81f33c618f2a1d3c006cbfcdec77abc",
            "riscv64-unknown-linux-gnu": "bb7252edaffd422bd1c044a4764dfcf83a5d7159942f445abbef524e54ea79a0",
            "s390x-unknown-linux-gnu": "03a90ffa9f92d4cf4caeefb9d15f0b39c05c1e60ade6688f32165f957db4f8f3",
            "x86_64-apple-darwin": "cee576de4919cd422dbc31eb85d3c145ee82acec84f651daaf32dc669b5149c9",
            "x86_64-pc-windows-msvc": "e538475ee249eacf63bfdae0e70af73e9c47360e6dd3d6825e7a35107e177de5",
            "aarch64-pc-windows-msvc": "39bc2fcac13aeba7d650f76badf63350a81c86167a62174cb092eab7a749f4a5",
            "x86_64-unknown-linux-gnu": "58addaabfab2de422180d32543fb3878ffc984c8a2e4005ff658a5cd83b31fc7",
            "x86_64-unknown-linux-musl": "dcf844400dc2e7f5f3604e994532e4d49db45f4deefe9afdf6809ca1bc6532ee",
            "aarch64-apple-darwin-freethreaded": "5b34488580df13df051a2e84e43cfca2ab28fdd7a61052f35988eb8b481b894a",
            "aarch64-unknown-linux-gnu-freethreaded": "0c2c83236f6e28c103e2660a82be94b2459ee8cfdd90f5dd82f0d503ca2aec09",
            "ppc64le-unknown-linux-gnu-freethreaded": "216842df2377fd032f279ded7fd23d7bdbd92d4c1fa7619523bc0dbdef5bd212",
            "riscv64-unknown-linux-gnu-freethreaded": "2a8b56f318d2e21b01b54909554c53d81871b9bb05d23ea7808dde9acec4dc7e",
            "s390x-unknown-linux-gnu-freethreaded": "06c4ca3983aad20723f68786e3663ab49fee1bf09326f341649205ed79d34fc6",
            "x86_64-apple-darwin-freethreaded": "4d8102b70ea9fe726ee3ae9ad9e9bc4cbe0b6ed18f7989c81aef81de578f0163",
            "x86_64-pc-windows-msvc-freethreaded": "6ff71bac78d650ce621fe6db49f06290e48bcceb61f69cccc7728584f70b6346",
            "aarch64-pc-windows-msvc-freethreaded": "3d99152b4e29b947fb1cfc8d035d1d511e50aeed72886ff4a5fd0a3694bd0b51",
            "x86_64-unknown-linux-gnu-freethreaded": "70f552e213734c0e260a57603bee504dd7ed0e78a10558b591e724ea8730fef5",
        },
        "strip_prefix": {
            "aarch64-apple-darwin": "python",
            "aarch64-unknown-linux-gnu": "python",
            "ppc64le-unknown-linux-gnu": "python",
            "s390x-unknown-linux-gnu": "python",
            "riscv64-unknown-linux-gnu": "python",
            "x86_64-apple-darwin": "python",
            "x86_64-pc-windows-msvc": "python",
            "aarch64-pc-windows-msvc": "python",
            "x86_64-unknown-linux-gnu": "python",
            "x86_64-unknown-linux-musl": "python",
            "aarch64-apple-darwin-freethreaded": "python/install",
            "aarch64-unknown-linux-gnu-freethreaded": "python/install",
            "ppc64le-unknown-linux-gnu-freethreaded": "python/install",
            "riscv64-unknown-linux-gnu-freethreaded": "python/install",
            "s390x-unknown-linux-gnu-freethreaded": "python/install",
            "x86_64-apple-darwin-freethreaded": "python/install",
            "x86_64-pc-windows-msvc-freethreaded": "python/install",
            "aarch64-pc-windows-msvc-freethreaded": "python/install",
            "x86_64-unknown-linux-gnu-freethreaded": "python/install",
        },
    },
}

# buildifier: disable=unsorted-dict-items
MINOR_MAPPING = {
    "3.9": "3.9.25",
    "3.10": "3.10.19",
    "3.11": "3.11.14",
    "3.12": "3.12.12",
    "3.13": "3.13.11",
    "3.14": "3.14.2",
    "3.15": "3.15.0a2",
}

def _generate_platforms():
    is_libc_glibc = str(Label("//python/config_settings:_is_py_linux_libc_glibc"))
    is_libc_musl = str(Label("//python/config_settings:_is_py_linux_libc_musl"))

    platforms = {
        "aarch64-apple-darwin": platform_info(
            compatible_with = [
                "@platforms//os:macos",
                "@platforms//cpu:aarch64",
            ],
            os_name = MACOS_NAME,
            arch = "aarch64",
        ),
        "aarch64-pc-windows-msvc": platform_info(
            compatible_with = [
                "@platforms//os:windows",
                "@platforms//cpu:aarch64",
            ],
            os_name = WINDOWS_NAME,
            arch = "aarch64",
        ),
        "aarch64-unknown-linux-gnu": platform_info(
            compatible_with = [
                "@platforms//os:linux",
                "@platforms//cpu:aarch64",
            ],
            target_settings = [
                is_libc_glibc,
            ],
            os_name = LINUX_NAME,
            arch = "aarch64",
        ),
        "arm64e-apple-darwin": platform_info(
            compatible_with = [
                "@platforms//os:macos",
                "@platforms//cpu:arm64e",
            ],
            os_name = MACOS_NAME,
            arch = "aarch64",
        ),
        "armv7-unknown-linux-gnu": platform_info(
            compatible_with = [
                "@platforms//os:linux",
                "@platforms//cpu:armv7",
            ],
            target_settings = [
                is_libc_glibc,
            ],
            os_name = LINUX_NAME,
            arch = "arm",
        ),
        "i386-unknown-linux-gnu": platform_info(
            compatible_with = [
                "@platforms//os:linux",
                "@platforms//cpu:i386",
            ],
            target_settings = [
                is_libc_glibc,
            ],
            os_name = LINUX_NAME,
            arch = "x86_32",
        ),
        "ppc64le-unknown-linux-gnu": platform_info(
            compatible_with = [
                "@platforms//os:linux",
                "@platforms//cpu:ppc",
            ],
            target_settings = [
                is_libc_glibc,
            ],
            os_name = LINUX_NAME,
            arch = "ppc",
        ),
        "riscv64-unknown-linux-gnu": platform_info(
            compatible_with = [
                "@platforms//os:linux",
                "@platforms//cpu:riscv64",
            ],
            target_settings = [
                is_libc_glibc,
            ],
            os_name = LINUX_NAME,
            arch = "riscv64",
        ),
        "s390x-unknown-linux-gnu": platform_info(
            compatible_with = [
                "@platforms//os:linux",
                "@platforms//cpu:s390x",
            ],
            target_settings = [
                is_libc_glibc,
            ],
            os_name = LINUX_NAME,
            arch = "s390x",
        ),
        "x86_64-apple-darwin": platform_info(
            compatible_with = [
                "@platforms//os:macos",
                "@platforms//cpu:x86_64",
            ],
            os_name = MACOS_NAME,
            arch = "x86_64",
        ),
        "x86_64-pc-windows-msvc": platform_info(
            compatible_with = [
                "@platforms//os:windows",
                "@platforms//cpu:x86_64",
            ],
            os_name = WINDOWS_NAME,
            arch = "x86_64",
        ),
        "x86_64-unknown-linux-gnu": platform_info(
            compatible_with = [
                "@platforms//os:linux",
                "@platforms//cpu:x86_64",
            ],
            target_settings = [
                is_libc_glibc,
            ],
            os_name = LINUX_NAME,
            arch = "x86_64",
        ),
        "x86_64-unknown-linux-musl": platform_info(
            compatible_with = [
                "@platforms//os:linux",
                "@platforms//cpu:x86_64",
            ],
            target_settings = [
                is_libc_musl,
            ],
            os_name = LINUX_NAME,
            arch = "x86_64",
        ),
    }

    is_freethreaded_yes = str(Label("//python/config_settings:_is_py_freethreaded_yes"))
    is_freethreaded_no = str(Label("//python/config_settings:_is_py_freethreaded_no"))
    return {
        p + suffix: platform_info(
            compatible_with = v.compatible_with,
            target_settings = [
                freethreadedness,
            ] + v.target_settings,
            os_name = v.os_name,
            arch = v.arch,
        )
        for p, v in platforms.items()
        for suffix, freethreadedness in {
            "": is_freethreaded_no,
            FREETHREADED: is_freethreaded_yes,
        }.items()
    }

PLATFORMS = _generate_platforms()

def get_release_info(platform, python_version, base_url = DEFAULT_RELEASE_BASE_URL, tool_versions = TOOL_VERSIONS):
    """Resolve the release URL for the requested interpreter version

    Args:
        platform: The platform string for the interpreter
        python_version: The version of the interpreter to get
        base_url: The URL to prepend to the 'url' attr in the tool_versions dict
        tool_versions: A dict listing the interpreter versions, their SHAs and URL

    Returns:
        A tuple of (filename, url, archive strip prefix, patches, patch_strip)
    """

    url = tool_versions[python_version]["url"]

    if type(url) == type({}):
        url = url[platform]

    if type(url) != type([]):
        url = [url]

    strip_prefix = tool_versions[python_version].get("strip_prefix", None)
    if type(strip_prefix) == type({}):
        strip_prefix = strip_prefix[platform]

    release_filename = None
    rendered_urls = []
    for u in url:
        p, _, _ = platform.partition(FREETHREADED)

        # Assume an unknown release_id is a newer url format
        release_id = 99999999
        url_parts = u.split("/")
        if len(url_parts) >= 2 and url_parts[-2].isdigit():
            maybe_release_id = url_parts[-2]
            release_id = int(maybe_release_id)

        if FREETHREADED.lstrip("-") in platform:
            build = "{}+{}-full".format(
                FREETHREADED.lstrip("-"),
                {
                    "aarch64-apple-darwin": "pgo+lto",
                    "aarch64-pc-windows-msvc": "pgo",
                    "aarch64-unknown-linux-gnu": "lto" if release_id < 20250702 else "pgo+lto",
                    "ppc64le-unknown-linux-gnu": "lto",
                    "riscv64-unknown-linux-gnu": "lto",
                    "s390x-unknown-linux-gnu": "lto",
                    "x86_64-apple-darwin": "pgo+lto",
                    "x86_64-pc-windows-msvc": "pgo",
                    "x86_64-unknown-linux-gnu": "pgo+lto",
                    "x86_64-unknown-linux-musl": "pgo+lto",
                }[p],
            )
        else:
            build = INSTALL_ONLY

        if WINDOWS_NAME in platform and release_id < 20250317:
            build = "shared-" + build

        release_filename = u.format(
            platform = p,
            python_version = python_version,
            build = build,
            ext = "tar.zst" if build.endswith("full") else "tar.gz",
        )
        if "://" in release_filename:  # is absolute url?
            rendered_urls.append(release_filename)
        else:
            rendered_urls.append("/".join([base_url, release_filename]))

    if release_filename == None:
        fail("release_filename should be set by now; were any download URLs given?")

    patches = tool_versions[python_version].get("patches", [])
    if type(patches) == type({}):
        if platform in patches.keys():
            patches = patches[platform]
        else:
            patches = []
    patch_strip = tool_versions[python_version].get("patch_strip", None)
    if type(patch_strip) == type({}):
        if platform in patch_strip.keys():
            patch_strip = patch_strip[platform]
        else:
            patch_strip = None

    return (release_filename, rendered_urls, strip_prefix, patches, patch_strip)

def gen_python_config_settings(name = ""):
    for platform in PLATFORMS.keys():
        native.config_setting(
            name = "{name}{platform}".format(name = name, platform = platform),
            flag_values = PLATFORMS[platform].flag_values,
            constraint_values = PLATFORMS[platform].compatible_with,
        )
