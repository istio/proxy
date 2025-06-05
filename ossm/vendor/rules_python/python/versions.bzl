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

# Values present in the @platforms//os package
MACOS_NAME = "osx"
LINUX_NAME = "linux"
WINDOWS_NAME = "windows"
FREETHREADED = "freethreaded"
INSTALL_ONLY = "install_only"

DEFAULT_RELEASE_BASE_URL = "https://github.com/indygreg/python-build-standalone/releases/download"

# When updating the versions and releases, run the following command to get
# the hashes:
#   bazel run //python/private:print_toolchains_checksums --//python/config_settings:python_version={major}.{minor}.{patch}
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
    "3.8.10": {
        "url": "20210506/cpython-{python_version}-{platform}-pgo+lto-20210506T0943.tar.zst",
        "sha256": {
            "x86_64-apple-darwin": "8d06bec08db8cdd0f64f4f05ee892cf2fcbc58cfb1dd69da2caab78fac420238",
            "x86_64-unknown-linux-gnu": "aec8c4c53373b90be7e2131093caa26063be6d9d826f599c935c0e1042af3355",
        },
        "strip_prefix": "python/install",
    },
    "3.8.12": {
        "url": "20220227/cpython-{python_version}+20220227-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "f9a3cbb81e0463d6615125964762d133387d561b226a30199f5b039b20f1d944",
            # no aarch64-unknown-linux-gnu build available for 3.8.12
            "x86_64-apple-darwin": "f323fbc558035c13a85ce2267d0fad9e89282268ecb810e364fff1d0a079d525",
            "x86_64-pc-windows-msvc": "4658e08a00d60b1e01559b74d58ff4dd04da6df935d55f6268a15d6d0a679d74",
            "x86_64-unknown-linux-gnu": "5be9c6d61e238b90dfd94755051c0d3a2d8023ebffdb4b0fa4e8fedd09a6cab6",
        },
        "strip_prefix": "python",
    },
    "3.8.13": {
        "url": "20220802/cpython-{python_version}+20220802-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "ae4131253d890b013171cb5f7b03cadc585ae263719506f7b7e063a7cf6fde76",
            # no aarch64-unknown-linux-gnu build available for 3.8.13
            "x86_64-apple-darwin": "cd6e7c0a27daf7df00f6882eaba01490dd963f698e99aeee9706877333e0df69",
            "x86_64-pc-windows-msvc": "f20643f1b3e263a56287319aea5c3888530c09ad9de3a5629b1a5d207807e6b9",
            "x86_64-unknown-linux-gnu": "fb566629ccb5f76ef56d275a3f8017d683f1c20c5beb5d5f38b155ed11e16187",
        },
        "strip_prefix": "python",
    },
    "3.8.15": {
        "url": "20221106/cpython-{python_version}+20221106-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "1e0a92d1a4f5e6d4a99f86b1cbf9773d703fe7fd032590f3e9c285c7a5eeb00a",
            "aarch64-unknown-linux-gnu": "886ab33ced13c84bf59ce8ff79eba6448365bfcafea1bf415bd1d75e21b690aa",
            "x86_64-apple-darwin": "70b57f28c2b5e1e3dd89f0d30edd5bc414e8b20195766cf328e1b26bed7890e1",
            "x86_64-pc-windows-msvc": "2fdc3fa1c95f982179bbbaedae2b328197658638799b6dcb63f9f494b0de59e2",
            "x86_64-unknown-linux-gnu": "e47edfb2ceaf43fc699e20c179ec428b6f3e497cf8e2dcd8e9c936d4b96b1e56",
        },
        "strip_prefix": "python",
    },
    "3.8.16": {
        "url": "20230116/cpython-{python_version}+20230116-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "d1f408569d8807c1053939d7822b082a17545e363697e1ce3cfb1ee75834c7be",
            "aarch64-unknown-linux-gnu": "15d00bc8400ed6d94c665a797dc8ed7a491ae25c5022e738dcd665cd29beec42",
            "x86_64-apple-darwin": "484ba901f64fc7888bec5994eb49343dc3f9d00ed43df17ee9c40935aad4aa18",
            "x86_64-pc-windows-msvc": "b446bec833eaba1bac9063bb9b4aeadfdf67fa81783b4487a90c56d408fb7994",
            "x86_64-unknown-linux-gnu": "c890de112f1ae31283a31fefd2061d5c97bdd4d1bdd795552c7abddef2697ea1",
        },
        "strip_prefix": "python",
    },
    "3.8.17": {
        "url": "20230826/cpython-{python_version}+20230826-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "c6f7a130d0044a78e39648f4dae56dcff5a41eba91888a99f6e560507162e6a1",
            "aarch64-unknown-linux-gnu": "9f6d585091fe26906ff1dbb80437a3fe37a1e3db34d6ecc0098f3d6a78356682",
            "x86_64-apple-darwin": "155b06821607bae1a58ecc60a7d036b358c766f19e493b8876190765c883a5c2",
            "x86_64-pc-windows-msvc": "6428e1b4e0b4482d390828de7d4c82815257443416cb786abe10cb2466ca68cd",
            "x86_64-unknown-linux-gnu": "8d3e1826c0bb7821ec63288038644808a2d45553245af106c685ef5892fabcd8",
        },
        "strip_prefix": "python",
    },
    "3.8.18": {
        "url": "20240224/cpython-{python_version}+20240224-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "4d493a1792bf211f37f98404cc1468f09bd781adc2602dea0df82ad264c11abc",
            "aarch64-unknown-linux-gnu": "6588c9eed93833d9483d01fe40ac8935f691a1af8e583d404ec7666631b52487",
            "x86_64-apple-darwin": "7d2cd8d289d5e3cdd0a8c06c028c7c621d3d00ce44b7e2f08c1724ae0471c626",
            "x86_64-pc-windows-msvc": "dba923ee5df8f99db04f599e826be92880746c02247c8d8e4d955d4bc711af11",
            "x86_64-unknown-linux-gnu": "5ae36825492372554c02708bdd26b8dcd57e3dbf34b3d6d599ad91d93540b2b7",
        },
        "strip_prefix": "python",
    },
    "3.8.19": {
        "url": "20240726/cpython-{python_version}+20240726-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "fe4af1b6bc59478d027ede43f6249cf7b9143558e171bdf8711247337623af57",
            "aarch64-unknown-linux-gnu": "8dc598aca7ad43ea20119324af98862d198d8990151c734a69f0fc9d16384b46",
            "x86_64-apple-darwin": "4bc990b35384c83b5b0b3071e91455ec203517e569f29f691b159f1a6b2a19b2",
            "x86_64-pc-windows-msvc": "4e8e9ddda82062d6e111108ab72f439acac4ba41b77d694548ef5dbf6b2b3319",
            "x86_64-unknown-linux-gnu": "e81ea4dd16e6057c8121bdbcb7b64e2956068ca019f244c814bc3ad907cb2765",
        },
        "strip_prefix": "python",
    },
    "3.8.20": {
        "url": "20241002/cpython-{python_version}+20241002-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "2ddfc04bdb3e240f30fb782fa1deec6323799d0e857e0b63fa299218658fd3d4",
            "aarch64-unknown-linux-gnu": "9d8798f9e79e0fc0f36fcb95bfa28a1023407d51a8ea5944b4da711f1f75f1ed",
            "x86_64-apple-darwin": "68d060cd373255d2ca5b8b3441363d5aa7cc45b0c11bbccf52b1717c2b5aa8bb",
            "x86_64-pc-windows-msvc": "41b6709fec9c56419b7de1940d1f87fa62045aff81734480672dcb807eedc47e",
            "x86_64-unknown-linux-gnu": "285e141c36f88b2e9357654c5f77d1f8fb29cc25132698fe35bb30d787f38e87",
        },
        "strip_prefix": "python",
    },
    "3.9.10": {
        "url": "20220227/cpython-{python_version}+20220227-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "ad66c2a3e7263147e046a32694de7b897a46fb0124409d29d3a93ede631c8aee",
            "aarch64-unknown-linux-gnu": "12dd1f125762f47975990ec744532a1cf3db74ad60f4dfb476ca42deb7f78ca4",
            "x86_64-apple-darwin": "fdaf594142446029e314a9beb91f1ac75af866320b50b8b968181e592550cd68",
            "x86_64-pc-windows-msvc": "c145d9d8143ce163670af124b623d7a2405143a3708b033b4d33eed355e61b24",
            "x86_64-unknown-linux-gnu": "455089cc576bd9a58db45e919d1fc867ecdbb0208067dffc845cc9bbf0701b70",
        },
        "strip_prefix": "python",
    },
    "3.9.12": {
        "url": "20220502/cpython-{python_version}+20220502-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "8dee06c07cc6429df34b6abe091a4684a86f7cec76f5d1ccc1c3ce2bd11168df",
            "aarch64-unknown-linux-gnu": "2ee1426c181e65133e57dc55c6a685cb1fb5e63ef02d684b8a667d5c031c4203",
            "x86_64-apple-darwin": "2453ba7f76b3df3310353b48c881d6cff622ba06e30d2b6ae91588b2bc9e481a",
            "x86_64-pc-windows-msvc": "3024147fd987d9e1b064a3d94932178ff8e0fe98cfea955704213c0762fee8df",
            "x86_64-unknown-linux-gnu": "ccca12f698b3b810d79c52f007078f520d588232a36bc12ede944ec3ea417816",
        },
        "strip_prefix": "python",
    },
    "3.9.13": {
        "url": "20220802/cpython-{python_version}+20220802-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "d9603edc296a2dcbc59d7ada780fd12527f05c3e0b99f7545112daf11636d6e5",
            "aarch64-unknown-linux-gnu": "80415aac1b96255b9211f6a4c300f31e9940c7e07a23d0dec12b53aa52c0d25e",
            "x86_64-apple-darwin": "9540a7efb7c8a54a48aff1cb9480e49588d9c0a3f934ad53f5b167338174afa3",
            "x86_64-pc-windows-msvc": "b538127025a467c64b3351babca2e4d2ea7bdfb7867d5febb3529c34456cdcd4",
            "x86_64-unknown-linux-gnu": "ce1cfca2715e7e646dd618a8cb9baff93000e345ccc979b801fc6ccde7ce97df",
        },
        "strip_prefix": "python",
    },
    "3.9.15": {
        "url": "20221106/cpython-{python_version}+20221106-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "64dc7e1013481c9864152c3dd806c41144c79d5e9cd3140e185c6a5060bdc9ab",
            "aarch64-unknown-linux-gnu": "52a8c0a67fb919f80962d992da1bddb511cdf92faf382701ce7673e10a8ff98f",
            "x86_64-apple-darwin": "f2bcade6fc976c472f18f2b3204d67202d43ae55cf6f9e670f95e488f780da08",
            "x86_64-pc-windows-msvc": "022daacab215679b87f0d200d08b9068a721605fa4721ebeda38220fc641ccf6",
            "x86_64-unknown-linux-gnu": "cdc3a4cfddcd63b6cebdd75b14970e02d8ef0ac5be4d350e57ab5df56c19e85e",
        },
        "strip_prefix": "python",
    },
    "3.9.16": {
        "url": "20230507/cpython-{python_version}+20230507-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "c1de1d854717a6245f45262ef1bb17b09e2c587590e7e3f406593c143ff875bd",
            "aarch64-unknown-linux-gnu": "f629b75ebfcafe9ceee2e796b7e4df5cf8dbd14f3c021afca078d159ab797acf",
            "ppc64le-unknown-linux-gnu": "ff3ac35c58f67839aff9b5185a976abd3d1abbe61af02089f7105e876c1fe284",
            "x86_64-apple-darwin": "3abc4d5fbbc80f5f848f280927ac5d13de8dc03aabb6ae65d8247cbb68e6f6bf",
            "x86_64-pc-windows-msvc": "cdabb47204e96ce7ea31fbd0b5ed586114dd7d8f8eddf60a509a7f70b48a1c5e",
            "x86_64-unknown-linux-gnu": "2b6e146234a4ef2a8946081fc3fbfffe0765b80b690425a49ebe40b47c33445b",
        },
        "strip_prefix": "python",
    },
    "3.9.17": {
        "url": "20230726/cpython-{python_version}+20230726-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "73dbe2d702210b566221da9265acc274ba15275c5d0d1fa327f44ad86cde9aa1",
            "aarch64-unknown-linux-gnu": "b77012ddaf7e0673e4aa4b1c5085275a06eee2d66f33442b5c54a12b62b96cbe",
            "ppc64le-unknown-linux-gnu": "c591a28d943dce5cf9833e916125fdfbeb3120270c4866ee214493ccb5b83c3c",
            "s390x-unknown-linux-gnu": "01454d7cc7c9c2fccde42ba868c4f372eaaafa48049d49dd94c9cf2875f497e6",
            "x86_64-apple-darwin": "dfe1bea92c94b9cb779288b0b06e39157c5ff7e465cdd24032ac147c2af485c0",
            "x86_64-pc-windows-msvc": "9b9a1e21eff29dcf043cea38180cf8ca3604b90117d00062a7b31605d4157714",
            "x86_64-unknown-linux-gnu": "26c4a712b4b8e11ed5c027db5654eb12927c02da4857b777afb98f7a930ce637",
        },
        "strip_prefix": "python",
    },
    "3.9.18": {
        "url": "20240224/cpython-{python_version}+20240224-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "2548f911a6e316575c303ba42bb51540dc9b47a9f76a06a2a37460d93b177aa2",
            "aarch64-unknown-linux-gnu": "e5bc5196baa603d635ee6b0cd141e359752ad3e8ea76127eb9141a3155c51200",
            "ppc64le-unknown-linux-gnu": "d6b18df7a25fe034fd5ce4e64216df2cc78b2d4d908d2a1c94058ae700d73d22",
            "s390x-unknown-linux-gnu": "15d059507c7e900e9665f31e8d903e5a24a68ceed24f9a1c5ac06ab42a354f3f",
            "x86_64-apple-darwin": "171d8b472fce0295be0e28bb702c43d5a2a39feccb3e72efe620ac3843c3e402",
            "x86_64-pc-windows-msvc": "a9bdbd728ed4c353a4157ecf74386117fb2a2769a9353f491c528371cfe7f6cd",
            "x86_64-unknown-linux-gnu": "0e5663025121186bd17d331538a44f48b41baff247891d014f3f962cbe2716b4",
        },
        "strip_prefix": "python",
    },
    "3.9.19": {
        "url": "20240726/cpython-{python_version}+20240726-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "0e5a7aae57c53d7a849bc7f67764a947b626e3fe8d4d41a8eed11d9e4be0b1c6",
            "aarch64-unknown-linux-gnu": "05ec896db9a9d4fe8004b4e4b6a6fdc588a015fedbddb475490885b0d9c7d9b3",
            "ppc64le-unknown-linux-gnu": "bfff0e3d536b2f0c315e85926cc317b7b756701b6de781a8972cefbdbc991ca2",
            "s390x-unknown-linux-gnu": "059ec97080b205ea5f1ddf71c18e22b691e8d68192bd37d13ad8f4359915299d",
            "x86_64-apple-darwin": "f2ae9fcac044a329739b8c1676245e8cb6b3094416220e71823d2673bdea0bdb",
            "x86_64-pc-windows-msvc": "a8df6a00140055c9accb0be632e7add951d587bbe3d63c40827bbd5145d8f557",
            "x86_64-unknown-linux-gnu": "cbf94cb1c9d4b5501d9b3652f6e8400c2cab7c41dfea48d344d9e7f29692b91b",
        },
        "strip_prefix": "python",
    },
    "3.9.20": {
        "url": "20241016/cpython-{python_version}+20241016-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "34ab2bc4c51502145e1a624b4e4ea06877e3d1934a88cc73ac2e0fd5fd439b75",
            "aarch64-unknown-linux-gnu": "1e486c054a4e86666cf24e04f5e29456324ba9c2b95bf1cae1805be90d3da154",
            "ppc64le-unknown-linux-gnu": "9a24ccdbfc7f67545d859128f02a3150a160ea6c2fc134b0773bf56f2d90b397",
            "s390x-unknown-linux-gnu": "2cee381069bf344fb20eba609af92dfe7ba67eb75bea08eeccf11048a2c380c0",
            "x86_64-apple-darwin": "193dc7f0284e4917d52b17a077924474882ee172872f2257cfe3375d6d468ed9",
            "x86_64-pc-windows-msvc": "5069008a237b90f6f7a86956903f2a0221b90d471daa6e4a94831eaa399e3993",
            "x86_64-unknown-linux-gnu": "c20ee831f7f46c58fa57919b75a40eb2b6a31e03fd29aaa4e8dab4b9c4b60d5d",
            "x86_64-unknown-linux-musl": "5c1cc348e317fe7af1acd6a7f665b46eccb554b20d6533f0e76c53f44d4556cc",
        },
        "strip_prefix": "python",
    },
    "3.9.21": {
        "url": "20241206/cpython-{python_version}+20241206-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "4bddc18228789d0316dcebc45b2242e0010fa6bc33c302b6b5a62a5ac39d2147",
            "aarch64-unknown-linux-gnu": "7d3b4ab90f73fa9dab0c350ca64b1caa9b8e4655913acd098e594473c49921c8",
            "ppc64le-unknown-linux-gnu": "966477345ca93f056cf18de9cff961aacda2318a8e641546e0fd7222f1362ee2",
            "s390x-unknown-linux-gnu": "3ba05a408edce4e20ebd116643c8418e62f7c8066c8a35fe8d3b78371d90b46a",
            "x86_64-apple-darwin": "619f5082288c771ad9b71e2daaf6df6bd39ca86e442638d150a71a6ccf62978d",
            "x86_64-pc-windows-msvc": "82736b5a185c57b296188ce778ed865ff10edc5fe9ff1ec4cb33b39ac8e4819c",
            "x86_64-unknown-linux-gnu": "208b2adc7c7e5d5df6d9385400dc7c4e3b4c3eed428e19a2326848978e98517e",
            "x86_64-unknown-linux-musl": "67c058dbaae8fd8c4f68e13b10805a9227918afc94326f21a9a2ec2daca3ddbd",
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
        "url": "20241206/cpython-{python_version}+20241206-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "c2d25840756127f3583b04b0697bef79edacb15f1402cd980292c93488c3df22",
            "aarch64-unknown-linux-gnu": "bbfc345615c5ed33916b4fd959fc16fa2e896a3c5eec1fb782c91b47c85c0542",
            "ppc64le-unknown-linux-gnu": "cb474b392733d5ac2adaa1cfcc2b63b957611dc26697e76822706cc61ac21515",
            "s390x-unknown-linux-gnu": "886a7effc8a3061d53cacc9cf54e82d6d57ac3665c258c6a2193528c16b557cd",
            "x86_64-apple-darwin": "31a110b631eb79103675ed556255045deeea5ff533296d7f35b4d195a0df0315",
            "x86_64-pc-windows-msvc": "fb7870717dc7e3aedcbab4a647782637da0046a4238db1d41eeaabb78566d814",
            "x86_64-unknown-linux-gnu": "b15de0d63eed9871ed57285f81fd123cf6c4117251a9cac8f81f9cf0cccc0a53",
            "x86_64-unknown-linux-musl": "bf956eeffcff002d2f38232faa750c279cbb76197b744761d1b253bf94d6f637",
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
    "3.11.11": {
        "url": "20241206/cpython-{python_version}+20241206-{platform}-{build}.tar.gz",
        "sha256": {
            "aarch64-apple-darwin": "566c5e266f2c933d0c0b213a75496bc6a090e493097802f809dbe21c75cd5d13",
            "aarch64-unknown-linux-gnu": "50ee364cfa24ee7d933eda955c9fe455bc0a8ebb9d998c9948f2909dac701dd9",
            "ppc64le-unknown-linux-gnu": "e0cdc00e42a05191b9b75ba976fc0fca9205c66fdaef7571c20532346fd3db1e",
            "s390x-unknown-linux-gnu": "3b106b8a3c5aa97ff76200cd0d9ba6eaed23d88ccb947e00ff6bb2d9f5422d2a",
            "x86_64-apple-darwin": "8ecd267281fb5b2464ddcd2de79622cfa7aff42e929b17989da2721ba39d4a5e",
            "x86_64-pc-windows-msvc": "d8986f026599074ddd206f3f62d6f2c323ca8fa7a854bf744989bfc0b12f5d0d",
            "x86_64-unknown-linux-gnu": "57a171af687c926c5cabe3d1c7ce9950b98f00b932accd596eb60e14ca39c42d",
            "x86_64-unknown-linux-musl": "8129a9a5c3f2654e1a9eed6093f5dc42399667b341050ff03219cb7df210c348",
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
}

# buildifier: disable=unsorted-dict-items
MINOR_MAPPING = {
    "3.8": "3.8.20",
    "3.9": "3.9.21",
    "3.10": "3.10.16",
    "3.11": "3.11.11",
    "3.12": "3.12.8",
    "3.13": "3.13.1",
}

def _generate_platforms():
    libc = Label("//python/config_settings:py_linux_libc")

    platforms = {
        "aarch64-apple-darwin": struct(
            compatible_with = [
                "@platforms//os:macos",
                "@platforms//cpu:aarch64",
            ],
            flag_values = {},
            os_name = MACOS_NAME,
            # Matches the value in @platforms//cpu package
            arch = "aarch64",
        ),
        "aarch64-unknown-linux-gnu": struct(
            compatible_with = [
                "@platforms//os:linux",
                "@platforms//cpu:aarch64",
            ],
            flag_values = {
                libc: "glibc",
            },
            os_name = LINUX_NAME,
            # Matches the value in @platforms//cpu package
            arch = "aarch64",
        ),
        "armv7-unknown-linux-gnu": struct(
            compatible_with = [
                "@platforms//os:linux",
                "@platforms//cpu:armv7",
            ],
            flag_values = {
                libc: "glibc",
            },
            os_name = LINUX_NAME,
            # Matches the value in @platforms//cpu package
            arch = "arm",
        ),
        "i386-unknown-linux-gnu": struct(
            compatible_with = [
                "@platforms//os:linux",
                "@platforms//cpu:i386",
            ],
            flag_values = {
                libc: "glibc",
            },
            os_name = LINUX_NAME,
            # Matches the value in @platforms//cpu package
            arch = "x86_32",
        ),
        "ppc64le-unknown-linux-gnu": struct(
            compatible_with = [
                "@platforms//os:linux",
                "@platforms//cpu:ppc",
            ],
            flag_values = {
                libc: "glibc",
            },
            os_name = LINUX_NAME,
            # Matches the value in @platforms//cpu package
            arch = "ppc",
        ),
        "riscv64-unknown-linux-gnu": struct(
            compatible_with = [
                "@platforms//os:linux",
                "@platforms//cpu:riscv64",
            ],
            flag_values = {
                Label("//python/config_settings:py_linux_libc"): "glibc",
            },
            os_name = LINUX_NAME,
            # Matches the value in @platforms//cpu package
            arch = "riscv64",
        ),
        "s390x-unknown-linux-gnu": struct(
            compatible_with = [
                "@platforms//os:linux",
                "@platforms//cpu:s390x",
            ],
            flag_values = {
                Label("//python/config_settings:py_linux_libc"): "glibc",
            },
            os_name = LINUX_NAME,
            # Matches the value in @platforms//cpu package
            arch = "s390x",
        ),
        "x86_64-apple-darwin": struct(
            compatible_with = [
                "@platforms//os:macos",
                "@platforms//cpu:x86_64",
            ],
            flag_values = {},
            os_name = MACOS_NAME,
            # Matches the value in @platforms//cpu package
            arch = "x86_64",
        ),
        "x86_64-pc-windows-msvc": struct(
            compatible_with = [
                "@platforms//os:windows",
                "@platforms//cpu:x86_64",
            ],
            flag_values = {},
            os_name = WINDOWS_NAME,
            # Matches the value in @platforms//cpu package
            arch = "x86_64",
        ),
        "x86_64-unknown-linux-gnu": struct(
            compatible_with = [
                "@platforms//os:linux",
                "@platforms//cpu:x86_64",
            ],
            flag_values = {
                libc: "glibc",
            },
            os_name = LINUX_NAME,
            # Matches the value in @platforms//cpu package
            arch = "x86_64",
        ),
        "x86_64-unknown-linux-musl": struct(
            compatible_with = [
                "@platforms//os:linux",
                "@platforms//cpu:x86_64",
            ],
            flag_values = {
                libc: "musl",
            },
            os_name = LINUX_NAME,
            arch = "x86_64",
        ),
    }

    freethreaded = Label("//python/config_settings:py_freethreaded")
    return {
        p + suffix: struct(
            compatible_with = v.compatible_with,
            flag_values = {
                freethreaded: freethreaded_value,
            } | v.flag_values,
            os_name = v.os_name,
            arch = v.arch,
        )
        for p, v in platforms.items()
        for suffix, freethreaded_value in {
            "": "no",
            "-" + FREETHREADED: "yes",
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
        p, _, _ = platform.partition("-" + FREETHREADED)

        if FREETHREADED in platform:
            build = "{}+{}-full".format(
                FREETHREADED,
                {
                    "aarch64-apple-darwin": "pgo+lto",
                    "aarch64-unknown-linux-gnu": "lto",
                    "ppc64le-unknown-linux-gnu": "lto",
                    "s390x-unknown-linux-gnu": "lto",
                    "x86_64-apple-darwin": "pgo+lto",
                    "x86_64-pc-windows-msvc": "pgo",
                    "x86_64-unknown-linux-gnu": "pgo+lto",
                }[p],
            )
        else:
            build = INSTALL_ONLY

        if WINDOWS_NAME in platform:
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

def print_toolchains_checksums(name):
    """A macro to print checksums for a particular Python interpreter version.

    Args:
        name: {type}`str`: the name of the runnable target.
    """
    all_commands = []
    by_version = {}
    for python_version in TOOL_VERSIONS.keys():
        by_version[python_version] = _commands_for_version(python_version)
        all_commands.append(_commands_for_version(python_version))

    template = """\
cat > "$@" <<'EOF'
#!/bin/bash

set -o errexit -o nounset -o pipefail

echo "Fetching hashes..."

{commands}
EOF
    """

    native.genrule(
        name = name,
        srcs = [],
        outs = ["print_toolchains_checksums.sh"],
        cmd = select({
            "//python/config_settings:is_python_{}".format(version): template.format(
                commands = commands,
            )
            for version, commands in by_version.items()
        } | {
            "//conditions:default": template.format(commands = "\n".join(all_commands)),
        }),
        executable = True,
    )

def _commands_for_version(python_version):
    return "\n".join([
        "echo \"{python_version}: {platform}: $$(curl --location --fail {release_url_sha256} 2>/dev/null || curl --location --fail {release_url} 2>/dev/null | shasum -a 256 | awk '{{ print $$1 }}')\"".format(
            python_version = python_version,
            platform = platform,
            release_url = release_url,
            release_url_sha256 = release_url + ".sha256",
        )
        for platform in TOOL_VERSIONS[python_version]["sha256"].keys()
        for release_url in get_release_info(platform, python_version)[1]
    ])

def gen_python_config_settings(name = ""):
    for platform in PLATFORMS.keys():
        native.config_setting(
            name = "{name}{platform}".format(name = name, platform = platform),
            flag_values = PLATFORMS[platform].flag_values,
            constraint_values = PLATFORMS[platform].compatible_with,
        )
