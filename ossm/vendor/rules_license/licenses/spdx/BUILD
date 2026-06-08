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

"""license_kind()s for SPDX license identifiers."""

# This is a set of license_kind declarations based on the SPDX license
# identifiers.  See: https://spdx.org/licenses/
#
# Open source projects using one of these well known licenses may reference
# the license_kind targets here. For example, their BUILD file might contain:
#
#   package(default_applicable_licenses = [":license"])
#
#   license(
#       name = "license",
#       license_kinds = ["@rules_license//licenses/spdx:MIT-0"],
#       license_text = "LICENSE.txt",
#   )
#
# Organizations building custom compliance checks based on these licenses
# declarations can take several approaches to align the the package author's
# declaration that the package is under (let's say) an MIT-0 license with
# the conditions that organization believes are required by MIT-0.
#
# One technique might be to:
#   1. Vendor in @rules_license to your source code tree.
#   2. Modify the targets here to add conditions for your particular needs.
#   3. Add it as a local_repository() to your Bazel WORKSPACE.
#   3. As the SPDX license list evolves, and your dependencies start using new
#      licenses, add new license_kind rules to this copy.
# Another possible strategy might be to
#   1. maintain a private list of these license_kind declarations somewhere
#      in your source tree. Let's say //compliance/licenses/spdx/BUILD.
#   2. Automatically remap "@rules_license//licenses/spdx:" to
#      "//compliance/licenses/spdx:" when importing third party code.
#
# The choice of approach should be based on your individual philosoply of how
# you should bring dependencies into your code. If you build from downloaded
# packages then you might favor the first approach, because external deps
# can simply refer to @rules_pkg, and they will pick up your local copy.
# If you maintain your own  third_party directory of all your dependences,
# so that your WORKSPACE files are empty, you might favor the second.

load("@rules_license//rules:license_kind.bzl", "license_kind")

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = ["//visibility:public"],
)

licenses(["notice"])

filegroup(
    name = "standard_package",
    srcs = ["BUILD"],
)

license_kind(
    name = "0BSD",
    conditions = [],
    url = "https://spdx.org/licenses/0BSD.html",
)

license_kind(
    name = "3D-Slicer-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/3D-Slicer-1.0.html",
)

license_kind(
    name = "AAL",
    conditions = [],
    url = "https://spdx.org/licenses/AAL.html",
)

license_kind(
    name = "Abstyles",
    conditions = [],
    url = "https://spdx.org/licenses/Abstyles.html",
)

license_kind(
    name = "AdaCore-doc",
    conditions = [],
    url = "https://spdx.org/licenses/AdaCore-doc.html",
)

license_kind(
    name = "Adobe-2006",
    conditions = [],
    url = "https://spdx.org/licenses/Adobe-2006.html",
)

license_kind(
    name = "Adobe-Display-PostScript",
    conditions = [],
    url = "https://spdx.org/licenses/Adobe-Display-PostScript.html",
)

license_kind(
    name = "Adobe-Glyph",
    conditions = [],
    url = "https://spdx.org/licenses/Adobe-Glyph.html",
)

license_kind(
    name = "Adobe-Utopia",
    conditions = [],
    url = "https://spdx.org/licenses/Adobe-Utopia.html",
)

license_kind(
    name = "ADSL",
    conditions = [],
    url = "https://spdx.org/licenses/ADSL.html",
)

license_kind(
    name = "AFL-1.1",
    conditions = [],
    url = "https://spdx.org/licenses/AFL-1.1.html",
)

license_kind(
    name = "AFL-1.2",
    conditions = [],
    url = "https://spdx.org/licenses/AFL-1.2.html",
)

license_kind(
    name = "AFL-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/AFL-2.0.html",
)

license_kind(
    name = "AFL-2.1",
    conditions = [],
    url = "https://spdx.org/licenses/AFL-2.1.html",
)

license_kind(
    name = "AFL-3.0",
    conditions = [],
    url = "https://spdx.org/licenses/AFL-3.0.html",
)

license_kind(
    name = "Afmparse",
    conditions = [],
    url = "https://spdx.org/licenses/Afmparse.html",
)

license_kind(
    name = "AGPL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/AGPL-1.0.html",
)

license_kind(
    name = "AGPL-1.0-only",
    conditions = [],
    url = "https://spdx.org/licenses/AGPL-1.0-only.html",
)

license_kind(
    name = "AGPL-1.0-or-later",
    conditions = [],
    url = "https://spdx.org/licenses/AGPL-1.0-or-later.html",
)

license_kind(
    name = "AGPL-3.0",
    conditions = [],
    url = "https://spdx.org/licenses/AGPL-3.0.html",
)

license_kind(
    name = "AGPL-3.0-only",
    conditions = [],
    url = "https://spdx.org/licenses/AGPL-3.0-only.html",
)

license_kind(
    name = "AGPL-3.0-or-later",
    conditions = [],
    url = "https://spdx.org/licenses/AGPL-3.0-or-later.html",
)

license_kind(
    name = "Aladdin",
    conditions = [],
    url = "https://spdx.org/licenses/Aladdin.html",
)

license_kind(
    name = "AMD-newlib",
    conditions = [],
    url = "https://spdx.org/licenses/AMD-newlib.html",
)

license_kind(
    name = "AMDPLPA",
    conditions = [],
    url = "https://spdx.org/licenses/AMDPLPA.html",
)

license_kind(
    name = "AML",
    conditions = [],
    url = "https://spdx.org/licenses/AML.html",
)

license_kind(
    name = "AML-glslang",
    conditions = [],
    url = "https://spdx.org/licenses/AML-glslang.html",
)

license_kind(
    name = "AMPAS",
    conditions = [],
    url = "https://spdx.org/licenses/AMPAS.html",
)

license_kind(
    name = "ANTLR-PD",
    conditions = [],
    url = "https://spdx.org/licenses/ANTLR-PD.html",
)

license_kind(
    name = "ANTLR-PD-fallback",
    conditions = [],
    url = "https://spdx.org/licenses/ANTLR-PD-fallback.html",
)

license_kind(
    name = "any-OSI",
    conditions = [],
    url = "https://spdx.org/licenses/any-OSI.html",
)

license_kind(
    name = "Apache-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/Apache-1.0.html",
)

license_kind(
    name = "Apache-1.1",
    conditions = [],
    url = "https://spdx.org/licenses/Apache-1.1.html",
)

license_kind(
    name = "Apache-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/Apache-2.0.html",
)

license_kind(
    name = "APAFML",
    conditions = [],
    url = "https://spdx.org/licenses/APAFML.html",
)

license_kind(
    name = "APL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/APL-1.0.html",
)

license_kind(
    name = "App-s2p",
    conditions = [],
    url = "https://spdx.org/licenses/App-s2p.html",
)

license_kind(
    name = "APSL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/APSL-1.0.html",
)

license_kind(
    name = "APSL-1.1",
    conditions = [],
    url = "https://spdx.org/licenses/APSL-1.1.html",
)

license_kind(
    name = "APSL-1.2",
    conditions = [],
    url = "https://spdx.org/licenses/APSL-1.2.html",
)

license_kind(
    name = "APSL-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/APSL-2.0.html",
)

license_kind(
    name = "Arphic-1999",
    conditions = [],
    url = "https://spdx.org/licenses/Arphic-1999.html",
)

license_kind(
    name = "Artistic-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/Artistic-1.0.html",
)

license_kind(
    name = "Artistic-1.0-cl8",
    conditions = [],
    url = "https://spdx.org/licenses/Artistic-1.0-cl8.html",
)

license_kind(
    name = "Artistic-1.0-Perl",
    conditions = [],
    url = "https://spdx.org/licenses/Artistic-1.0-Perl.html",
)

license_kind(
    name = "Artistic-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/Artistic-2.0.html",
)

license_kind(
    name = "ASWF-Digital-Assets-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/ASWF-Digital-Assets-1.0.html",
)

license_kind(
    name = "ASWF-Digital-Assets-1.1",
    conditions = [],
    url = "https://spdx.org/licenses/ASWF-Digital-Assets-1.1.html",
)

license_kind(
    name = "Baekmuk",
    conditions = [],
    url = "https://spdx.org/licenses/Baekmuk.html",
)

license_kind(
    name = "Bahyph",
    conditions = [],
    url = "https://spdx.org/licenses/Bahyph.html",
)

license_kind(
    name = "Barr",
    conditions = [],
    url = "https://spdx.org/licenses/Barr.html",
)

license_kind(
    name = "bcrypt-Solar-Designer",
    conditions = [],
    url = "https://spdx.org/licenses/bcrypt-Solar-Designer.html",
)

license_kind(
    name = "Beerware",
    conditions = [],
    url = "https://spdx.org/licenses/Beerware.html",
)

license_kind(
    name = "Bitstream-Charter",
    conditions = [],
    url = "https://spdx.org/licenses/Bitstream-Charter.html",
)

license_kind(
    name = "Bitstream-Vera",
    conditions = [],
    url = "https://spdx.org/licenses/Bitstream-Vera.html",
)

license_kind(
    name = "BitTorrent-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/BitTorrent-1.0.html",
)

license_kind(
    name = "BitTorrent-1.1",
    conditions = [],
    url = "https://spdx.org/licenses/BitTorrent-1.1.html",
)

license_kind(
    name = "blessing",
    conditions = [],
    url = "https://spdx.org/licenses/blessing.html",
)

license_kind(
    name = "BlueOak-1.0.0",
    conditions = [],
    url = "https://spdx.org/licenses/BlueOak-1.0.0.html",
)

license_kind(
    name = "Boehm-GC",
    conditions = [],
    url = "https://spdx.org/licenses/Boehm-GC.html",
)

license_kind(
    name = "Borceux",
    conditions = [],
    url = "https://spdx.org/licenses/Borceux.html",
)

license_kind(
    name = "Brian-Gladman-2-Clause",
    conditions = [],
    url = "https://spdx.org/licenses/Brian-Gladman-2-Clause.html",
)

license_kind(
    name = "Brian-Gladman-3-Clause",
    conditions = [],
    url = "https://spdx.org/licenses/Brian-Gladman-3-Clause.html",
)

license_kind(
    name = "BSD-1-Clause",
    conditions = [],
    url = "https://spdx.org/licenses/BSD-1-Clause.html",
)

license_kind(
    name = "BSD-2-Clause",
    conditions = [],
    url = "https://spdx.org/licenses/BSD-2-Clause.html",
)

license_kind(
    name = "BSD-2-Clause-Darwin",
    conditions = [],
    url = "https://spdx.org/licenses/BSD-2-Clause-Darwin.html",
)

license_kind(
    name = "BSD-2-Clause-first-lines",
    conditions = [],
    url = "https://spdx.org/licenses/BSD-2-Clause-first-lines.html",
)

license_kind(
    name = "BSD-2-Clause-FreeBSD",
    conditions = [],
    url = "https://spdx.org/licenses/BSD-2-Clause-FreeBSD.html",
)

license_kind(
    name = "BSD-2-Clause-NetBSD",
    conditions = [],
    url = "https://spdx.org/licenses/BSD-2-Clause-NetBSD.html",
)

license_kind(
    name = "BSD-2-Clause-Patent",
    conditions = [],
    url = "https://spdx.org/licenses/BSD-2-Clause-Patent.html",
)

license_kind(
    name = "BSD-2-Clause-Views",
    conditions = [],
    url = "https://spdx.org/licenses/BSD-2-Clause-Views.html",
)

license_kind(
    name = "BSD-3-Clause",
    conditions = [],
    url = "https://spdx.org/licenses/BSD-3-Clause.html",
)

license_kind(
    name = "BSD-3-Clause-acpica",
    conditions = [],
    url = "https://spdx.org/licenses/BSD-3-Clause-acpica.html",
)

license_kind(
    name = "BSD-3-Clause-Attribution",
    conditions = [],
    url = "https://spdx.org/licenses/BSD-3-Clause-Attribution.html",
)

license_kind(
    name = "BSD-3-Clause-Clear",
    conditions = [],
    url = "https://spdx.org/licenses/BSD-3-Clause-Clear.html",
)

license_kind(
    name = "BSD-3-Clause-flex",
    conditions = [],
    url = "https://spdx.org/licenses/BSD-3-Clause-flex.html",
)

license_kind(
    name = "BSD-3-Clause-HP",
    conditions = [],
    url = "https://spdx.org/licenses/BSD-3-Clause-HP.html",
)

license_kind(
    name = "BSD-3-Clause-LBNL",
    conditions = [],
    url = "https://spdx.org/licenses/BSD-3-Clause-LBNL.html",
)

license_kind(
    name = "BSD-3-Clause-Modification",
    conditions = [],
    url = "https://spdx.org/licenses/BSD-3-Clause-Modification.html",
)

license_kind(
    name = "BSD-3-Clause-No-Military-License",
    conditions = [],
    url = "https://spdx.org/licenses/BSD-3-Clause-No-Military-License.html",
)

license_kind(
    name = "BSD-3-Clause-No-Nuclear-License",
    conditions = [],
    url = "https://spdx.org/licenses/BSD-3-Clause-No-Nuclear-License.html",
)

license_kind(
    name = "BSD-3-Clause-No-Nuclear-License-2014",
    conditions = [],
    url = "https://spdx.org/licenses/BSD-3-Clause-No-Nuclear-License-2014.html",
)

license_kind(
    name = "BSD-3-Clause-No-Nuclear-Warranty",
    conditions = [],
    url = "https://spdx.org/licenses/BSD-3-Clause-No-Nuclear-Warranty.html",
)

license_kind(
    name = "BSD-3-Clause-Open-MPI",
    conditions = [],
    url = "https://spdx.org/licenses/BSD-3-Clause-Open-MPI.html",
)

license_kind(
    name = "BSD-3-Clause-Sun",
    conditions = [],
    url = "https://spdx.org/licenses/BSD-3-Clause-Sun.html",
)

license_kind(
    name = "BSD-4-Clause",
    conditions = [],
    url = "https://spdx.org/licenses/BSD-4-Clause.html",
)

license_kind(
    name = "BSD-4-Clause-Shortened",
    conditions = [],
    url = "https://spdx.org/licenses/BSD-4-Clause-Shortened.html",
)

license_kind(
    name = "BSD-4-Clause-UC",
    conditions = [],
    url = "https://spdx.org/licenses/BSD-4-Clause-UC.html",
)

license_kind(
    name = "BSD-4.3RENO",
    conditions = [],
    url = "https://spdx.org/licenses/BSD-4.3RENO.html",
)

license_kind(
    name = "BSD-4.3TAHOE",
    conditions = [],
    url = "https://spdx.org/licenses/BSD-4.3TAHOE.html",
)

license_kind(
    name = "BSD-Advertising-Acknowledgement",
    conditions = [],
    url = "https://spdx.org/licenses/BSD-Advertising-Acknowledgement.html",
)

license_kind(
    name = "BSD-Attribution-HPND-disclaimer",
    conditions = [],
    url = "https://spdx.org/licenses/BSD-Attribution-HPND-disclaimer.html",
)

license_kind(
    name = "BSD-Inferno-Nettverk",
    conditions = [],
    url = "https://spdx.org/licenses/BSD-Inferno-Nettverk.html",
)

license_kind(
    name = "BSD-Protection",
    conditions = [],
    url = "https://spdx.org/licenses/BSD-Protection.html",
)

license_kind(
    name = "BSD-Source-beginning-file",
    conditions = [],
    url = "https://spdx.org/licenses/BSD-Source-beginning-file.html",
)

license_kind(
    name = "BSD-Source-Code",
    conditions = [],
    url = "https://spdx.org/licenses/BSD-Source-Code.html",
)

license_kind(
    name = "BSD-Systemics",
    conditions = [],
    url = "https://spdx.org/licenses/BSD-Systemics.html",
)

license_kind(
    name = "BSD-Systemics-W3Works",
    conditions = [],
    url = "https://spdx.org/licenses/BSD-Systemics-W3Works.html",
)

license_kind(
    name = "BSL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/BSL-1.0.html",
)

license_kind(
    name = "BUSL-1.1",
    conditions = [],
    url = "https://spdx.org/licenses/BUSL-1.1.html",
)

license_kind(
    name = "bzip2-1.0.5",
    conditions = [],
    url = "https://spdx.org/licenses/bzip2-1.0.5.html",
)

license_kind(
    name = "bzip2-1.0.6",
    conditions = [],
    url = "https://spdx.org/licenses/bzip2-1.0.6.html",
)

license_kind(
    name = "C-UDA-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/C-UDA-1.0.html",
)

license_kind(
    name = "CAL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/CAL-1.0.html",
)

license_kind(
    name = "CAL-1.0-Combined-Work-Exception",
    conditions = [],
    url = "https://spdx.org/licenses/CAL-1.0-Combined-Work-Exception.html",
)

license_kind(
    name = "Caldera",
    conditions = [],
    url = "https://spdx.org/licenses/Caldera.html",
)

license_kind(
    name = "Caldera-no-preamble",
    conditions = [],
    url = "https://spdx.org/licenses/Caldera-no-preamble.html",
)

license_kind(
    name = "Catharon",
    conditions = [],
    url = "https://spdx.org/licenses/Catharon.html",
)

license_kind(
    name = "CATOSL-1.1",
    conditions = [],
    url = "https://spdx.org/licenses/CATOSL-1.1.html",
)

license_kind(
    name = "CC-BY-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-1.0.html",
)

license_kind(
    name = "CC-BY-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-2.0.html",
)

license_kind(
    name = "CC-BY-2.5",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-2.5.html",
)

license_kind(
    name = "CC-BY-2.5-AU",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-2.5-AU.html",
)

license_kind(
    name = "CC-BY-3.0",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-3.0.html",
)

license_kind(
    name = "CC-BY-3.0-AT",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-3.0-AT.html",
)

license_kind(
    name = "CC-BY-3.0-AU",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-3.0-AU.html",
)

license_kind(
    name = "CC-BY-3.0-DE",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-3.0-DE.html",
)

license_kind(
    name = "CC-BY-3.0-IGO",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-3.0-IGO.html",
)

license_kind(
    name = "CC-BY-3.0-NL",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-3.0-NL.html",
)

license_kind(
    name = "CC-BY-3.0-US",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-3.0-US.html",
)

license_kind(
    name = "CC-BY-4.0",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-4.0.html",
)

license_kind(
    name = "CC-BY-NC-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-NC-1.0.html",
)

license_kind(
    name = "CC-BY-NC-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-NC-2.0.html",
)

license_kind(
    name = "CC-BY-NC-2.5",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-NC-2.5.html",
)

license_kind(
    name = "CC-BY-NC-3.0",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-NC-3.0.html",
)

license_kind(
    name = "CC-BY-NC-3.0-DE",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-NC-3.0-DE.html",
)

license_kind(
    name = "CC-BY-NC-4.0",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-NC-4.0.html",
)

license_kind(
    name = "CC-BY-NC-ND-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-NC-ND-1.0.html",
)

license_kind(
    name = "CC-BY-NC-ND-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-NC-ND-2.0.html",
)

license_kind(
    name = "CC-BY-NC-ND-2.5",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-NC-ND-2.5.html",
)

license_kind(
    name = "CC-BY-NC-ND-3.0",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-NC-ND-3.0.html",
)

license_kind(
    name = "CC-BY-NC-ND-3.0-DE",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-NC-ND-3.0-DE.html",
)

license_kind(
    name = "CC-BY-NC-ND-3.0-IGO",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-NC-ND-3.0-IGO.html",
)

license_kind(
    name = "CC-BY-NC-ND-4.0",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-NC-ND-4.0.html",
)

license_kind(
    name = "CC-BY-NC-SA-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-NC-SA-1.0.html",
)

license_kind(
    name = "CC-BY-NC-SA-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-NC-SA-2.0.html",
)

license_kind(
    name = "CC-BY-NC-SA-2.0-DE",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-NC-SA-2.0-DE.html",
)

license_kind(
    name = "CC-BY-NC-SA-2.0-FR",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-NC-SA-2.0-FR.html",
)

license_kind(
    name = "CC-BY-NC-SA-2.0-UK",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-NC-SA-2.0-UK.html",
)

license_kind(
    name = "CC-BY-NC-SA-2.5",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-NC-SA-2.5.html",
)

license_kind(
    name = "CC-BY-NC-SA-3.0",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-NC-SA-3.0.html",
)

license_kind(
    name = "CC-BY-NC-SA-3.0-DE",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-NC-SA-3.0-DE.html",
)

license_kind(
    name = "CC-BY-NC-SA-3.0-IGO",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-NC-SA-3.0-IGO.html",
)

license_kind(
    name = "CC-BY-NC-SA-4.0",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-NC-SA-4.0.html",
)

license_kind(
    name = "CC-BY-ND-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-ND-1.0.html",
)

license_kind(
    name = "CC-BY-ND-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-ND-2.0.html",
)

license_kind(
    name = "CC-BY-ND-2.5",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-ND-2.5.html",
)

license_kind(
    name = "CC-BY-ND-3.0",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-ND-3.0.html",
)

license_kind(
    name = "CC-BY-ND-3.0-DE",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-ND-3.0-DE.html",
)

license_kind(
    name = "CC-BY-ND-4.0",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-ND-4.0.html",
)

license_kind(
    name = "CC-BY-SA-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-SA-1.0.html",
)

license_kind(
    name = "CC-BY-SA-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-SA-2.0.html",
)

license_kind(
    name = "CC-BY-SA-2.0-UK",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-SA-2.0-UK.html",
)

license_kind(
    name = "CC-BY-SA-2.1-JP",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-SA-2.1-JP.html",
)

license_kind(
    name = "CC-BY-SA-2.5",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-SA-2.5.html",
)

license_kind(
    name = "CC-BY-SA-3.0",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-SA-3.0.html",
)

license_kind(
    name = "CC-BY-SA-3.0-AT",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-SA-3.0-AT.html",
)

license_kind(
    name = "CC-BY-SA-3.0-DE",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-SA-3.0-DE.html",
)

license_kind(
    name = "CC-BY-SA-3.0-IGO",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-SA-3.0-IGO.html",
)

license_kind(
    name = "CC-BY-SA-4.0",
    conditions = [],
    url = "https://spdx.org/licenses/CC-BY-SA-4.0.html",
)

license_kind(
    name = "CC-PDDC",
    conditions = [],
    url = "https://spdx.org/licenses/CC-PDDC.html",
)

license_kind(
    name = "CC0-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/CC0-1.0.html",
)

license_kind(
    name = "CDDL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/CDDL-1.0.html",
)

license_kind(
    name = "CDDL-1.1",
    conditions = [],
    url = "https://spdx.org/licenses/CDDL-1.1.html",
)

license_kind(
    name = "CDL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/CDL-1.0.html",
)

license_kind(
    name = "CDLA-Permissive-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/CDLA-Permissive-1.0.html",
)

license_kind(
    name = "CDLA-Permissive-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/CDLA-Permissive-2.0.html",
)

license_kind(
    name = "CDLA-Sharing-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/CDLA-Sharing-1.0.html",
)

license_kind(
    name = "CECILL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/CECILL-1.0.html",
)

license_kind(
    name = "CECILL-1.1",
    conditions = [],
    url = "https://spdx.org/licenses/CECILL-1.1.html",
)

license_kind(
    name = "CECILL-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/CECILL-2.0.html",
)

license_kind(
    name = "CECILL-2.1",
    conditions = [],
    url = "https://spdx.org/licenses/CECILL-2.1.html",
)

license_kind(
    name = "CECILL-B",
    conditions = [],
    url = "https://spdx.org/licenses/CECILL-B.html",
)

license_kind(
    name = "CECILL-C",
    conditions = [],
    url = "https://spdx.org/licenses/CECILL-C.html",
)

license_kind(
    name = "CERN-OHL-1.1",
    conditions = [],
    url = "https://spdx.org/licenses/CERN-OHL-1.1.html",
)

license_kind(
    name = "CERN-OHL-1.2",
    conditions = [],
    url = "https://spdx.org/licenses/CERN-OHL-1.2.html",
)

license_kind(
    name = "CERN-OHL-P-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/CERN-OHL-P-2.0.html",
)

license_kind(
    name = "CERN-OHL-S-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/CERN-OHL-S-2.0.html",
)

license_kind(
    name = "CERN-OHL-W-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/CERN-OHL-W-2.0.html",
)

license_kind(
    name = "CFITSIO",
    conditions = [],
    url = "https://spdx.org/licenses/CFITSIO.html",
)

license_kind(
    name = "check-cvs",
    conditions = [],
    url = "https://spdx.org/licenses/check-cvs.html",
)

license_kind(
    name = "checkmk",
    conditions = [],
    url = "https://spdx.org/licenses/checkmk.html",
)

license_kind(
    name = "ClArtistic",
    conditions = [],
    url = "https://spdx.org/licenses/ClArtistic.html",
)

license_kind(
    name = "Clips",
    conditions = [],
    url = "https://spdx.org/licenses/Clips.html",
)

license_kind(
    name = "CMU-Mach",
    conditions = [],
    url = "https://spdx.org/licenses/CMU-Mach.html",
)

license_kind(
    name = "CMU-Mach-nodoc",
    conditions = [],
    url = "https://spdx.org/licenses/CMU-Mach-nodoc.html",
)

license_kind(
    name = "CNRI-Jython",
    conditions = [],
    url = "https://spdx.org/licenses/CNRI-Jython.html",
)

license_kind(
    name = "CNRI-Python",
    conditions = [],
    url = "https://spdx.org/licenses/CNRI-Python.html",
)

license_kind(
    name = "CNRI-Python-GPL-Compatible",
    conditions = [],
    url = "https://spdx.org/licenses/CNRI-Python-GPL-Compatible.html",
)

license_kind(
    name = "COIL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/COIL-1.0.html",
)

license_kind(
    name = "Community-Spec-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/Community-Spec-1.0.html",
)

license_kind(
    name = "Condor-1.1",
    conditions = [],
    url = "https://spdx.org/licenses/Condor-1.1.html",
)

license_kind(
    name = "copyleft-next-0.3.0",
    conditions = [],
    url = "https://spdx.org/licenses/copyleft-next-0.3.0.html",
)

license_kind(
    name = "copyleft-next-0.3.1",
    conditions = [],
    url = "https://spdx.org/licenses/copyleft-next-0.3.1.html",
)

license_kind(
    name = "Cornell-Lossless-JPEG",
    conditions = [],
    url = "https://spdx.org/licenses/Cornell-Lossless-JPEG.html",
)

license_kind(
    name = "CPAL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/CPAL-1.0.html",
)

license_kind(
    name = "CPL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/CPL-1.0.html",
)

license_kind(
    name = "CPOL-1.02",
    conditions = [],
    url = "https://spdx.org/licenses/CPOL-1.02.html",
)

license_kind(
    name = "Cronyx",
    conditions = [],
    url = "https://spdx.org/licenses/Cronyx.html",
)

license_kind(
    name = "Crossword",
    conditions = [],
    url = "https://spdx.org/licenses/Crossword.html",
)

license_kind(
    name = "CrystalStacker",
    conditions = [],
    url = "https://spdx.org/licenses/CrystalStacker.html",
)

license_kind(
    name = "CUA-OPL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/CUA-OPL-1.0.html",
)

license_kind(
    name = "Cube",
    conditions = [],
    url = "https://spdx.org/licenses/Cube.html",
)

license_kind(
    name = "curl",
    conditions = [],
    url = "https://spdx.org/licenses/curl.html",
)

license_kind(
    name = "cve-tou",
    conditions = [],
    url = "https://spdx.org/licenses/cve-tou.html",
)

license_kind(
    name = "D-FSL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/D-FSL-1.0.html",
)

license_kind(
    name = "DEC-3-Clause",
    conditions = [],
    url = "https://spdx.org/licenses/DEC-3-Clause.html",
)

license_kind(
    name = "diffmark",
    conditions = [],
    url = "https://spdx.org/licenses/diffmark.html",
)

license_kind(
    name = "DL-DE-BY-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/DL-DE-BY-2.0.html",
)

license_kind(
    name = "DL-DE-ZERO-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/DL-DE-ZERO-2.0.html",
)

license_kind(
    name = "DOC",
    conditions = [],
    url = "https://spdx.org/licenses/DOC.html",
)

license_kind(
    name = "DocBook-Schema",
    conditions = [],
    url = "https://spdx.org/licenses/DocBook-Schema.html",
)

license_kind(
    name = "DocBook-Stylesheet",
    conditions = [],
    url = "https://spdx.org/licenses/DocBook-Stylesheet.html",
)

license_kind(
    name = "DocBook-XML",
    conditions = [],
    url = "https://spdx.org/licenses/DocBook-XML.html",
)

license_kind(
    name = "Dotseqn",
    conditions = [],
    url = "https://spdx.org/licenses/Dotseqn.html",
)

license_kind(
    name = "DRL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/DRL-1.0.html",
)

license_kind(
    name = "DRL-1.1",
    conditions = [],
    url = "https://spdx.org/licenses/DRL-1.1.html",
)

license_kind(
    name = "DSDP",
    conditions = [],
    url = "https://spdx.org/licenses/DSDP.html",
)

license_kind(
    name = "dtoa",
    conditions = [],
    url = "https://spdx.org/licenses/dtoa.html",
)

license_kind(
    name = "dvipdfm",
    conditions = [],
    url = "https://spdx.org/licenses/dvipdfm.html",
)

license_kind(
    name = "ECL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/ECL-1.0.html",
)

license_kind(
    name = "ECL-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/ECL-2.0.html",
)

license_kind(
    name = "eCos-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/eCos-2.0.html",
)

license_kind(
    name = "EFL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/EFL-1.0.html",
)

license_kind(
    name = "EFL-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/EFL-2.0.html",
)

license_kind(
    name = "eGenix",
    conditions = [],
    url = "https://spdx.org/licenses/eGenix.html",
)

license_kind(
    name = "Elastic-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/Elastic-2.0.html",
)

license_kind(
    name = "Entessa",
    conditions = [],
    url = "https://spdx.org/licenses/Entessa.html",
)

license_kind(
    name = "EPICS",
    conditions = [],
    url = "https://spdx.org/licenses/EPICS.html",
)

license_kind(
    name = "EPL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/EPL-1.0.html",
)

license_kind(
    name = "EPL-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/EPL-2.0.html",
)

license_kind(
    name = "ErlPL-1.1",
    conditions = [],
    url = "https://spdx.org/licenses/ErlPL-1.1.html",
)

license_kind(
    name = "etalab-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/etalab-2.0.html",
)

license_kind(
    name = "EUDatagrid",
    conditions = [],
    url = "https://spdx.org/licenses/EUDatagrid.html",
)

license_kind(
    name = "EUPL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/EUPL-1.0.html",
)

license_kind(
    name = "EUPL-1.1",
    conditions = [],
    url = "https://spdx.org/licenses/EUPL-1.1.html",
)

license_kind(
    name = "EUPL-1.2",
    conditions = [],
    url = "https://spdx.org/licenses/EUPL-1.2.html",
)

license_kind(
    name = "Eurosym",
    conditions = [],
    url = "https://spdx.org/licenses/Eurosym.html",
)

license_kind(
    name = "Fair",
    conditions = [],
    url = "https://spdx.org/licenses/Fair.html",
)

license_kind(
    name = "FBM",
    conditions = [],
    url = "https://spdx.org/licenses/FBM.html",
)

license_kind(
    name = "FDK-AAC",
    conditions = [],
    url = "https://spdx.org/licenses/FDK-AAC.html",
)

license_kind(
    name = "Ferguson-Twofish",
    conditions = [],
    url = "https://spdx.org/licenses/Ferguson-Twofish.html",
)

license_kind(
    name = "Frameworx-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/Frameworx-1.0.html",
)

license_kind(
    name = "FreeBSD-DOC",
    conditions = [],
    url = "https://spdx.org/licenses/FreeBSD-DOC.html",
)

license_kind(
    name = "FreeImage",
    conditions = [],
    url = "https://spdx.org/licenses/FreeImage.html",
)

license_kind(
    name = "FSFAP",
    conditions = [],
    url = "https://spdx.org/licenses/FSFAP.html",
)

license_kind(
    name = "FSFAP-no-warranty-disclaimer",
    conditions = [],
    url = "https://spdx.org/licenses/FSFAP-no-warranty-disclaimer.html",
)

license_kind(
    name = "FSFUL",
    conditions = [],
    url = "https://spdx.org/licenses/FSFUL.html",
)

license_kind(
    name = "FSFULLR",
    conditions = [],
    url = "https://spdx.org/licenses/FSFULLR.html",
)

license_kind(
    name = "FSFULLRWD",
    conditions = [],
    url = "https://spdx.org/licenses/FSFULLRWD.html",
)

license_kind(
    name = "FTL",
    conditions = [],
    url = "https://spdx.org/licenses/FTL.html",
)

license_kind(
    name = "Furuseth",
    conditions = [],
    url = "https://spdx.org/licenses/Furuseth.html",
)

license_kind(
    name = "fwlw",
    conditions = [],
    url = "https://spdx.org/licenses/fwlw.html",
)

license_kind(
    name = "GCR-docs",
    conditions = [],
    url = "https://spdx.org/licenses/GCR-docs.html",
)

license_kind(
    name = "GD",
    conditions = [],
    url = "https://spdx.org/licenses/GD.html",
)

license_kind(
    name = "GFDL-1.1",
    conditions = [],
    url = "https://spdx.org/licenses/GFDL-1.1.html",
)

license_kind(
    name = "GFDL-1.1-invariants-only",
    conditions = [],
    url = "https://spdx.org/licenses/GFDL-1.1-invariants-only.html",
)

license_kind(
    name = "GFDL-1.1-invariants-or-later",
    conditions = [],
    url = "https://spdx.org/licenses/GFDL-1.1-invariants-or-later.html",
)

license_kind(
    name = "GFDL-1.1-no-invariants-only",
    conditions = [],
    url = "https://spdx.org/licenses/GFDL-1.1-no-invariants-only.html",
)

license_kind(
    name = "GFDL-1.1-no-invariants-or-later",
    conditions = [],
    url = "https://spdx.org/licenses/GFDL-1.1-no-invariants-or-later.html",
)

license_kind(
    name = "GFDL-1.1-only",
    conditions = [],
    url = "https://spdx.org/licenses/GFDL-1.1-only.html",
)

license_kind(
    name = "GFDL-1.1-or-later",
    conditions = [],
    url = "https://spdx.org/licenses/GFDL-1.1-or-later.html",
)

license_kind(
    name = "GFDL-1.2",
    conditions = [],
    url = "https://spdx.org/licenses/GFDL-1.2.html",
)

license_kind(
    name = "GFDL-1.2-invariants-only",
    conditions = [],
    url = "https://spdx.org/licenses/GFDL-1.2-invariants-only.html",
)

license_kind(
    name = "GFDL-1.2-invariants-or-later",
    conditions = [],
    url = "https://spdx.org/licenses/GFDL-1.2-invariants-or-later.html",
)

license_kind(
    name = "GFDL-1.2-no-invariants-only",
    conditions = [],
    url = "https://spdx.org/licenses/GFDL-1.2-no-invariants-only.html",
)

license_kind(
    name = "GFDL-1.2-no-invariants-or-later",
    conditions = [],
    url = "https://spdx.org/licenses/GFDL-1.2-no-invariants-or-later.html",
)

license_kind(
    name = "GFDL-1.2-only",
    conditions = [],
    url = "https://spdx.org/licenses/GFDL-1.2-only.html",
)

license_kind(
    name = "GFDL-1.2-or-later",
    conditions = [],
    url = "https://spdx.org/licenses/GFDL-1.2-or-later.html",
)

license_kind(
    name = "GFDL-1.3",
    conditions = [],
    url = "https://spdx.org/licenses/GFDL-1.3.html",
)

license_kind(
    name = "GFDL-1.3-invariants-only",
    conditions = [],
    url = "https://spdx.org/licenses/GFDL-1.3-invariants-only.html",
)

license_kind(
    name = "GFDL-1.3-invariants-or-later",
    conditions = [],
    url = "https://spdx.org/licenses/GFDL-1.3-invariants-or-later.html",
)

license_kind(
    name = "GFDL-1.3-no-invariants-only",
    conditions = [],
    url = "https://spdx.org/licenses/GFDL-1.3-no-invariants-only.html",
)

license_kind(
    name = "GFDL-1.3-no-invariants-or-later",
    conditions = [],
    url = "https://spdx.org/licenses/GFDL-1.3-no-invariants-or-later.html",
)

license_kind(
    name = "GFDL-1.3-only",
    conditions = [],
    url = "https://spdx.org/licenses/GFDL-1.3-only.html",
)

license_kind(
    name = "GFDL-1.3-or-later",
    conditions = [],
    url = "https://spdx.org/licenses/GFDL-1.3-or-later.html",
)

license_kind(
    name = "Giftware",
    conditions = [],
    url = "https://spdx.org/licenses/Giftware.html",
)

license_kind(
    name = "GL2PS",
    conditions = [],
    url = "https://spdx.org/licenses/GL2PS.html",
)

license_kind(
    name = "Glide",
    conditions = [],
    url = "https://spdx.org/licenses/Glide.html",
)

license_kind(
    name = "Glulxe",
    conditions = [],
    url = "https://spdx.org/licenses/Glulxe.html",
)

license_kind(
    name = "GLWTPL",
    conditions = [],
    url = "https://spdx.org/licenses/GLWTPL.html",
)

license_kind(
    name = "gnuplot",
    conditions = [],
    url = "https://spdx.org/licenses/gnuplot.html",
)

license_kind(
    name = "GPL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/GPL-1.0.html",
)

license_kind(
    name = "GPL-1.0+",
    conditions = [],
    url = "https://spdx.org/licenses/GPL-1.0+.html",
)

license_kind(
    name = "GPL-1.0-only",
    conditions = [],
    url = "https://spdx.org/licenses/GPL-1.0-only.html",
)

license_kind(
    name = "GPL-1.0-or-later",
    conditions = [],
    url = "https://spdx.org/licenses/GPL-1.0-or-later.html",
)

license_kind(
    name = "GPL-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/GPL-2.0.html",
)

license_kind(
    name = "GPL-2.0+",
    conditions = [],
    url = "https://spdx.org/licenses/GPL-2.0+.html",
)

license_kind(
    name = "GPL-2.0-only",
    conditions = [],
    url = "https://spdx.org/licenses/GPL-2.0-only.html",
)

license_kind(
    name = "GPL-2.0-or-later",
    conditions = [],
    url = "https://spdx.org/licenses/GPL-2.0-or-later.html",
)

license_kind(
    name = "GPL-2.0-with-autoconf-exception",
    conditions = [],
    url = "https://spdx.org/licenses/GPL-2.0-with-autoconf-exception.html",
)

license_kind(
    name = "GPL-2.0-with-bison-exception",
    conditions = [],
    url = "https://spdx.org/licenses/GPL-2.0-with-bison-exception.html",
)

license_kind(
    name = "GPL-2.0-with-classpath-exception",
    conditions = [],
    url = "https://spdx.org/licenses/GPL-2.0-with-classpath-exception.html",
)

license_kind(
    name = "GPL-2.0-with-font-exception",
    conditions = [],
    url = "https://spdx.org/licenses/GPL-2.0-with-font-exception.html",
)

license_kind(
    name = "GPL-2.0-with-GCC-exception",
    conditions = [],
    url = "https://spdx.org/licenses/GPL-2.0-with-GCC-exception.html",
)

license_kind(
    name = "GPL-3.0",
    conditions = [],
    url = "https://spdx.org/licenses/GPL-3.0.html",
)

license_kind(
    name = "GPL-3.0+",
    conditions = [],
    url = "https://spdx.org/licenses/GPL-3.0+.html",
)

license_kind(
    name = "GPL-3.0-only",
    conditions = [],
    url = "https://spdx.org/licenses/GPL-3.0-only.html",
)

license_kind(
    name = "GPL-3.0-or-later",
    conditions = [],
    url = "https://spdx.org/licenses/GPL-3.0-or-later.html",
)

license_kind(
    name = "GPL-3.0-with-autoconf-exception",
    conditions = [],
    url = "https://spdx.org/licenses/GPL-3.0-with-autoconf-exception.html",
)

license_kind(
    name = "GPL-3.0-with-GCC-exception",
    conditions = [],
    url = "https://spdx.org/licenses/GPL-3.0-with-GCC-exception.html",
)

license_kind(
    name = "Graphics-Gems",
    conditions = [],
    url = "https://spdx.org/licenses/Graphics-Gems.html",
)

license_kind(
    name = "gSOAP-1.3b",
    conditions = [],
    url = "https://spdx.org/licenses/gSOAP-1.3b.html",
)

license_kind(
    name = "gtkbook",
    conditions = [],
    url = "https://spdx.org/licenses/gtkbook.html",
)

license_kind(
    name = "Gutmann",
    conditions = [],
    url = "https://spdx.org/licenses/Gutmann.html",
)

license_kind(
    name = "HaskellReport",
    conditions = [],
    url = "https://spdx.org/licenses/HaskellReport.html",
)

license_kind(
    name = "hdparm",
    conditions = [],
    url = "https://spdx.org/licenses/hdparm.html",
)

license_kind(
    name = "HIDAPI",
    conditions = [],
    url = "https://spdx.org/licenses/HIDAPI.html",
)

license_kind(
    name = "Hippocratic-2.1",
    conditions = [],
    url = "https://spdx.org/licenses/Hippocratic-2.1.html",
)

license_kind(
    name = "HP-1986",
    conditions = [],
    url = "https://spdx.org/licenses/HP-1986.html",
)

license_kind(
    name = "HP-1989",
    conditions = [],
    url = "https://spdx.org/licenses/HP-1989.html",
)

license_kind(
    name = "HPND",
    conditions = [],
    url = "https://spdx.org/licenses/HPND.html",
)

license_kind(
    name = "HPND-DEC",
    conditions = [],
    url = "https://spdx.org/licenses/HPND-DEC.html",
)

license_kind(
    name = "HPND-doc",
    conditions = [],
    url = "https://spdx.org/licenses/HPND-doc.html",
)

license_kind(
    name = "HPND-doc-sell",
    conditions = [],
    url = "https://spdx.org/licenses/HPND-doc-sell.html",
)

license_kind(
    name = "HPND-export-US",
    conditions = [],
    url = "https://spdx.org/licenses/HPND-export-US.html",
)

license_kind(
    name = "HPND-export-US-acknowledgement",
    conditions = [],
    url = "https://spdx.org/licenses/HPND-export-US-acknowledgement.html",
)

license_kind(
    name = "HPND-export-US-modify",
    conditions = [],
    url = "https://spdx.org/licenses/HPND-export-US-modify.html",
)

license_kind(
    name = "HPND-export2-US",
    conditions = [],
    url = "https://spdx.org/licenses/HPND-export2-US.html",
)

license_kind(
    name = "HPND-Fenneberg-Livingston",
    conditions = [],
    url = "https://spdx.org/licenses/HPND-Fenneberg-Livingston.html",
)

license_kind(
    name = "HPND-INRIA-IMAG",
    conditions = [],
    url = "https://spdx.org/licenses/HPND-INRIA-IMAG.html",
)

license_kind(
    name = "HPND-Intel",
    conditions = [],
    url = "https://spdx.org/licenses/HPND-Intel.html",
)

license_kind(
    name = "HPND-Kevlin-Henney",
    conditions = [],
    url = "https://spdx.org/licenses/HPND-Kevlin-Henney.html",
)

license_kind(
    name = "HPND-Markus-Kuhn",
    conditions = [],
    url = "https://spdx.org/licenses/HPND-Markus-Kuhn.html",
)

license_kind(
    name = "HPND-merchantability-variant",
    conditions = [],
    url = "https://spdx.org/licenses/HPND-merchantability-variant.html",
)

license_kind(
    name = "HPND-MIT-disclaimer",
    conditions = [],
    url = "https://spdx.org/licenses/HPND-MIT-disclaimer.html",
)

license_kind(
    name = "HPND-Netrek",
    conditions = [],
    url = "https://spdx.org/licenses/HPND-Netrek.html",
)

license_kind(
    name = "HPND-Pbmplus",
    conditions = [],
    url = "https://spdx.org/licenses/HPND-Pbmplus.html",
)

license_kind(
    name = "HPND-sell-MIT-disclaimer-xserver",
    conditions = [],
    url = "https://spdx.org/licenses/HPND-sell-MIT-disclaimer-xserver.html",
)

license_kind(
    name = "HPND-sell-regexpr",
    conditions = [],
    url = "https://spdx.org/licenses/HPND-sell-regexpr.html",
)

license_kind(
    name = "HPND-sell-variant",
    conditions = [],
    url = "https://spdx.org/licenses/HPND-sell-variant.html",
)

license_kind(
    name = "HPND-sell-variant-MIT-disclaimer",
    conditions = [],
    url = "https://spdx.org/licenses/HPND-sell-variant-MIT-disclaimer.html",
)

license_kind(
    name = "HPND-sell-variant-MIT-disclaimer-rev",
    conditions = [],
    url = "https://spdx.org/licenses/HPND-sell-variant-MIT-disclaimer-rev.html",
)

license_kind(
    name = "HPND-UC",
    conditions = [],
    url = "https://spdx.org/licenses/HPND-UC.html",
)

license_kind(
    name = "HPND-UC-export-US",
    conditions = [],
    url = "https://spdx.org/licenses/HPND-UC-export-US.html",
)

license_kind(
    name = "HTMLTIDY",
    conditions = [],
    url = "https://spdx.org/licenses/HTMLTIDY.html",
)

license_kind(
    name = "IBM-pibs",
    conditions = [],
    url = "https://spdx.org/licenses/IBM-pibs.html",
)

license_kind(
    name = "ICU",
    conditions = [],
    url = "https://spdx.org/licenses/ICU.html",
)

license_kind(
    name = "IEC-Code-Components-EULA",
    conditions = [],
    url = "https://spdx.org/licenses/IEC-Code-Components-EULA.html",
)

license_kind(
    name = "IJG",
    conditions = [],
    url = "https://spdx.org/licenses/IJG.html",
)

license_kind(
    name = "IJG-short",
    conditions = [],
    url = "https://spdx.org/licenses/IJG-short.html",
)

license_kind(
    name = "ImageMagick",
    conditions = [],
    url = "https://spdx.org/licenses/ImageMagick.html",
)

license_kind(
    name = "iMatix",
    conditions = [],
    url = "https://spdx.org/licenses/iMatix.html",
)

license_kind(
    name = "Imlib2",
    conditions = [],
    url = "https://spdx.org/licenses/Imlib2.html",
)

license_kind(
    name = "Info-ZIP",
    conditions = [],
    url = "https://spdx.org/licenses/Info-ZIP.html",
)

license_kind(
    name = "Inner-Net-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/Inner-Net-2.0.html",
)

license_kind(
    name = "Intel",
    conditions = [],
    url = "https://spdx.org/licenses/Intel.html",
)

license_kind(
    name = "Intel-ACPI",
    conditions = [],
    url = "https://spdx.org/licenses/Intel-ACPI.html",
)

license_kind(
    name = "Interbase-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/Interbase-1.0.html",
)

license_kind(
    name = "IPA",
    conditions = [],
    url = "https://spdx.org/licenses/IPA.html",
)

license_kind(
    name = "IPL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/IPL-1.0.html",
)

license_kind(
    name = "ISC",
    conditions = [],
    url = "https://spdx.org/licenses/ISC.html",
)

license_kind(
    name = "ISC-Veillard",
    conditions = [],
    url = "https://spdx.org/licenses/ISC-Veillard.html",
)

license_kind(
    name = "Jam",
    conditions = [],
    url = "https://spdx.org/licenses/Jam.html",
)

license_kind(
    name = "JasPer-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/JasPer-2.0.html",
)

license_kind(
    name = "JPL-image",
    conditions = [],
    url = "https://spdx.org/licenses/JPL-image.html",
)

license_kind(
    name = "JPNIC",
    conditions = [],
    url = "https://spdx.org/licenses/JPNIC.html",
)

license_kind(
    name = "JSON",
    conditions = [],
    url = "https://spdx.org/licenses/JSON.html",
)

license_kind(
    name = "Kastrup",
    conditions = [],
    url = "https://spdx.org/licenses/Kastrup.html",
)

license_kind(
    name = "Kazlib",
    conditions = [],
    url = "https://spdx.org/licenses/Kazlib.html",
)

license_kind(
    name = "KiCad-libraries-exception",
    conditions = [],
    url = "https://spdx.org/licenses/KiCad-libraries-exception.html",
)

license_kind(
    name = "Knuth-CTAN",
    conditions = [],
    url = "https://spdx.org/licenses/Knuth-CTAN.html",
)

license_kind(
    name = "LAL-1.2",
    conditions = [],
    url = "https://spdx.org/licenses/LAL-1.2.html",
)

license_kind(
    name = "LAL-1.3",
    conditions = [],
    url = "https://spdx.org/licenses/LAL-1.3.html",
)

license_kind(
    name = "Latex2e",
    conditions = [],
    url = "https://spdx.org/licenses/Latex2e.html",
)

license_kind(
    name = "Latex2e-translated-notice",
    conditions = [],
    url = "https://spdx.org/licenses/Latex2e-translated-notice.html",
)

license_kind(
    name = "Leptonica",
    conditions = [],
    url = "https://spdx.org/licenses/Leptonica.html",
)

license_kind(
    name = "LGPL-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/LGPL-2.0.html",
)

license_kind(
    name = "LGPL-2.0+",
    conditions = [],
    url = "https://spdx.org/licenses/LGPL-2.0+.html",
)

license_kind(
    name = "LGPL-2.0-only",
    conditions = [],
    url = "https://spdx.org/licenses/LGPL-2.0-only.html",
)

license_kind(
    name = "LGPL-2.0-or-later",
    conditions = [],
    url = "https://spdx.org/licenses/LGPL-2.0-or-later.html",
)

license_kind(
    name = "LGPL-2.1",
    conditions = [],
    url = "https://spdx.org/licenses/LGPL-2.1.html",
)

license_kind(
    name = "LGPL-2.1+",
    conditions = [],
    url = "https://spdx.org/licenses/LGPL-2.1+.html",
)

license_kind(
    name = "LGPL-2.1-only",
    conditions = [],
    url = "https://spdx.org/licenses/LGPL-2.1-only.html",
)

license_kind(
    name = "LGPL-2.1-or-later",
    conditions = [],
    url = "https://spdx.org/licenses/LGPL-2.1-or-later.html",
)

license_kind(
    name = "LGPL-3.0",
    conditions = [],
    url = "https://spdx.org/licenses/LGPL-3.0.html",
)

license_kind(
    name = "LGPL-3.0+",
    conditions = [],
    url = "https://spdx.org/licenses/LGPL-3.0+.html",
)

license_kind(
    name = "LGPL-3.0-only",
    conditions = [],
    url = "https://spdx.org/licenses/LGPL-3.0-only.html",
)

license_kind(
    name = "LGPL-3.0-or-later",
    conditions = [],
    url = "https://spdx.org/licenses/LGPL-3.0-or-later.html",
)

license_kind(
    name = "LGPLLR",
    conditions = [],
    url = "https://spdx.org/licenses/LGPLLR.html",
)

license_kind(
    name = "Libpng",
    conditions = [],
    url = "https://spdx.org/licenses/Libpng.html",
)

license_kind(
    name = "libpng-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/libpng-2.0.html",
)

license_kind(
    name = "libselinux-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/libselinux-1.0.html",
)

license_kind(
    name = "libtiff",
    conditions = [],
    url = "https://spdx.org/licenses/libtiff.html",
)

license_kind(
    name = "libutil-David-Nugent",
    conditions = [],
    url = "https://spdx.org/licenses/libutil-David-Nugent.html",
)

license_kind(
    name = "LiLiQ-P-1.1",
    conditions = [],
    url = "https://spdx.org/licenses/LiLiQ-P-1.1.html",
)

license_kind(
    name = "LiLiQ-R-1.1",
    conditions = [],
    url = "https://spdx.org/licenses/LiLiQ-R-1.1.html",
)

license_kind(
    name = "LiLiQ-Rplus-1.1",
    conditions = [],
    url = "https://spdx.org/licenses/LiLiQ-Rplus-1.1.html",
)

license_kind(
    name = "Linux-man-pages-1-para",
    conditions = [],
    url = "https://spdx.org/licenses/Linux-man-pages-1-para.html",
)

license_kind(
    name = "Linux-man-pages-copyleft",
    conditions = [],
    url = "https://spdx.org/licenses/Linux-man-pages-copyleft.html",
)

license_kind(
    name = "Linux-man-pages-copyleft-2-para",
    conditions = [],
    url = "https://spdx.org/licenses/Linux-man-pages-copyleft-2-para.html",
)

license_kind(
    name = "Linux-man-pages-copyleft-var",
    conditions = [],
    url = "https://spdx.org/licenses/Linux-man-pages-copyleft-var.html",
)

license_kind(
    name = "Linux-OpenIB",
    conditions = [],
    url = "https://spdx.org/licenses/Linux-OpenIB.html",
)

license_kind(
    name = "LOOP",
    conditions = [],
    url = "https://spdx.org/licenses/LOOP.html",
)

license_kind(
    name = "LPD-document",
    conditions = [],
    url = "https://spdx.org/licenses/LPD-document.html",
)

license_kind(
    name = "LPL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/LPL-1.0.html",
)

license_kind(
    name = "LPL-1.02",
    conditions = [],
    url = "https://spdx.org/licenses/LPL-1.02.html",
)

license_kind(
    name = "LPPL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/LPPL-1.0.html",
)

license_kind(
    name = "LPPL-1.1",
    conditions = [],
    url = "https://spdx.org/licenses/LPPL-1.1.html",
)

license_kind(
    name = "LPPL-1.2",
    conditions = [],
    url = "https://spdx.org/licenses/LPPL-1.2.html",
)

license_kind(
    name = "LPPL-1.3a",
    conditions = [],
    url = "https://spdx.org/licenses/LPPL-1.3a.html",
)

license_kind(
    name = "LPPL-1.3c",
    conditions = [],
    url = "https://spdx.org/licenses/LPPL-1.3c.html",
)

license_kind(
    name = "lsof",
    conditions = [],
    url = "https://spdx.org/licenses/lsof.html",
)

license_kind(
    name = "Lucida-Bitmap-Fonts",
    conditions = [],
    url = "https://spdx.org/licenses/Lucida-Bitmap-Fonts.html",
)

license_kind(
    name = "LZMA-SDK-9.11-to-9.20",
    conditions = [],
    url = "https://spdx.org/licenses/LZMA-SDK-9.11-to-9.20.html",
)

license_kind(
    name = "LZMA-SDK-9.22",
    conditions = [],
    url = "https://spdx.org/licenses/LZMA-SDK-9.22.html",
)

license_kind(
    name = "Mackerras-3-Clause",
    conditions = [],
    url = "https://spdx.org/licenses/Mackerras-3-Clause.html",
)

license_kind(
    name = "Mackerras-3-Clause-acknowledgment",
    conditions = [],
    url = "https://spdx.org/licenses/Mackerras-3-Clause-acknowledgment.html",
)

license_kind(
    name = "magaz",
    conditions = [],
    url = "https://spdx.org/licenses/magaz.html",
)

license_kind(
    name = "mailprio",
    conditions = [],
    url = "https://spdx.org/licenses/mailprio.html",
)

license_kind(
    name = "MakeIndex",
    conditions = [],
    url = "https://spdx.org/licenses/MakeIndex.html",
)

license_kind(
    name = "Martin-Birgmeier",
    conditions = [],
    url = "https://spdx.org/licenses/Martin-Birgmeier.html",
)

license_kind(
    name = "McPhee-slideshow",
    conditions = [],
    url = "https://spdx.org/licenses/McPhee-slideshow.html",
)

license_kind(
    name = "metamail",
    conditions = [],
    url = "https://spdx.org/licenses/metamail.html",
)

license_kind(
    name = "Minpack",
    conditions = [],
    url = "https://spdx.org/licenses/Minpack.html",
)

license_kind(
    name = "MirOS",
    conditions = [],
    url = "https://spdx.org/licenses/MirOS.html",
)

license_kind(
    name = "MIT",
    conditions = [],
    url = "https://spdx.org/licenses/MIT.html",
)

license_kind(
    name = "MIT-0",
    conditions = [],
    url = "https://spdx.org/licenses/MIT-0.html",
)

license_kind(
    name = "MIT-advertising",
    conditions = [],
    url = "https://spdx.org/licenses/MIT-advertising.html",
)

license_kind(
    name = "MIT-Click",
    conditions = [],
    url = "https://spdx.org/licenses/MIT-Click.html",
)

license_kind(
    name = "MIT-CMU",
    conditions = [],
    url = "https://spdx.org/licenses/MIT-CMU.html",
)

license_kind(
    name = "MIT-enna",
    conditions = [],
    url = "https://spdx.org/licenses/MIT-enna.html",
)

license_kind(
    name = "MIT-feh",
    conditions = [],
    url = "https://spdx.org/licenses/MIT-feh.html",
)

license_kind(
    name = "MIT-Festival",
    conditions = [],
    url = "https://spdx.org/licenses/MIT-Festival.html",
)

license_kind(
    name = "MIT-Khronos-old",
    conditions = [],
    url = "https://spdx.org/licenses/MIT-Khronos-old.html",
)

license_kind(
    name = "MIT-Modern-Variant",
    conditions = [],
    url = "https://spdx.org/licenses/MIT-Modern-Variant.html",
)

license_kind(
    name = "MIT-open-group",
    conditions = [],
    url = "https://spdx.org/licenses/MIT-open-group.html",
)

license_kind(
    name = "MIT-testregex",
    conditions = [],
    url = "https://spdx.org/licenses/MIT-testregex.html",
)

license_kind(
    name = "MIT-Wu",
    conditions = [],
    url = "https://spdx.org/licenses/MIT-Wu.html",
)

license_kind(
    name = "MITNFA",
    conditions = [],
    url = "https://spdx.org/licenses/MITNFA.html",
)

license_kind(
    name = "MMIXware",
    conditions = [],
    url = "https://spdx.org/licenses/MMIXware.html",
)

license_kind(
    name = "Motosoto",
    conditions = [],
    url = "https://spdx.org/licenses/Motosoto.html",
)

license_kind(
    name = "MPEG-SSG",
    conditions = [],
    url = "https://spdx.org/licenses/MPEG-SSG.html",
)

license_kind(
    name = "mpi-permissive",
    conditions = [],
    url = "https://spdx.org/licenses/mpi-permissive.html",
)

license_kind(
    name = "mpich2",
    conditions = [],
    url = "https://spdx.org/licenses/mpich2.html",
)

license_kind(
    name = "MPL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/MPL-1.0.html",
)

license_kind(
    name = "MPL-1.1",
    conditions = [],
    url = "https://spdx.org/licenses/MPL-1.1.html",
)

license_kind(
    name = "MPL-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/MPL-2.0.html",
)

license_kind(
    name = "MPL-2.0-no-copyleft-exception",
    conditions = [],
    url = "https://spdx.org/licenses/MPL-2.0-no-copyleft-exception.html",
)

license_kind(
    name = "mplus",
    conditions = [],
    url = "https://spdx.org/licenses/mplus.html",
)

license_kind(
    name = "MS-LPL",
    conditions = [],
    url = "https://spdx.org/licenses/MS-LPL.html",
)

license_kind(
    name = "MS-PL",
    conditions = [],
    url = "https://spdx.org/licenses/MS-PL.html",
)

license_kind(
    name = "MS-RL",
    conditions = [],
    url = "https://spdx.org/licenses/MS-RL.html",
)

license_kind(
    name = "MTLL",
    conditions = [],
    url = "https://spdx.org/licenses/MTLL.html",
)

license_kind(
    name = "MulanPSL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/MulanPSL-1.0.html",
)

license_kind(
    name = "MulanPSL-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/MulanPSL-2.0.html",
)

license_kind(
    name = "Multics",
    conditions = [],
    url = "https://spdx.org/licenses/Multics.html",
)

license_kind(
    name = "Mup",
    conditions = [],
    url = "https://spdx.org/licenses/Mup.html",
)

license_kind(
    name = "NAIST-2003",
    conditions = [],
    url = "https://spdx.org/licenses/NAIST-2003.html",
)

license_kind(
    name = "NASA-1.3",
    conditions = [],
    url = "https://spdx.org/licenses/NASA-1.3.html",
)

license_kind(
    name = "Naumen",
    conditions = [],
    url = "https://spdx.org/licenses/Naumen.html",
)

license_kind(
    name = "NBPL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/NBPL-1.0.html",
)

license_kind(
    name = "NCBI-PD",
    conditions = [],
    url = "https://spdx.org/licenses/NCBI-PD.html",
)

license_kind(
    name = "NCGL-UK-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/NCGL-UK-2.0.html",
)

license_kind(
    name = "NCL",
    conditions = [],
    url = "https://spdx.org/licenses/NCL.html",
)

license_kind(
    name = "NCSA",
    conditions = [],
    url = "https://spdx.org/licenses/NCSA.html",
)

license_kind(
    name = "Net-SNMP",
    conditions = [],
    url = "https://spdx.org/licenses/Net-SNMP.html",
)

license_kind(
    name = "NetCDF",
    conditions = [],
    url = "https://spdx.org/licenses/NetCDF.html",
)

license_kind(
    name = "Newsletr",
    conditions = [],
    url = "https://spdx.org/licenses/Newsletr.html",
)

license_kind(
    name = "NGPL",
    conditions = [],
    url = "https://spdx.org/licenses/NGPL.html",
)

license_kind(
    name = "NICTA-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/NICTA-1.0.html",
)

license_kind(
    name = "NIST-PD",
    conditions = [],
    url = "https://spdx.org/licenses/NIST-PD.html",
)

license_kind(
    name = "NIST-PD-fallback",
    conditions = [],
    url = "https://spdx.org/licenses/NIST-PD-fallback.html",
)

license_kind(
    name = "NIST-Software",
    conditions = [],
    url = "https://spdx.org/licenses/NIST-Software.html",
)

license_kind(
    name = "NLOD-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/NLOD-1.0.html",
)

license_kind(
    name = "NLOD-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/NLOD-2.0.html",
)

license_kind(
    name = "NLPL",
    conditions = [],
    url = "https://spdx.org/licenses/NLPL.html",
)

license_kind(
    name = "Nokia",
    conditions = [],
    url = "https://spdx.org/licenses/Nokia.html",
)

license_kind(
    name = "NOSL",
    conditions = [],
    url = "https://spdx.org/licenses/NOSL.html",
)

license_kind(
    name = "Noweb",
    conditions = [],
    url = "https://spdx.org/licenses/Noweb.html",
)

license_kind(
    name = "NPL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/NPL-1.0.html",
)

license_kind(
    name = "NPL-1.1",
    conditions = [],
    url = "https://spdx.org/licenses/NPL-1.1.html",
)

license_kind(
    name = "NPOSL-3.0",
    conditions = [],
    url = "https://spdx.org/licenses/NPOSL-3.0.html",
)

license_kind(
    name = "NRL",
    conditions = [],
    url = "https://spdx.org/licenses/NRL.html",
)

license_kind(
    name = "NTP",
    conditions = [],
    url = "https://spdx.org/licenses/NTP.html",
)

license_kind(
    name = "NTP-0",
    conditions = [],
    url = "https://spdx.org/licenses/NTP-0.html",
)

license_kind(
    name = "Nunit",
    conditions = [],
    url = "https://spdx.org/licenses/Nunit.html",
)

license_kind(
    name = "O-UDA-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/O-UDA-1.0.html",
)

license_kind(
    name = "OAR",
    conditions = [],
    url = "https://spdx.org/licenses/OAR.html",
)

license_kind(
    name = "OCCT-PL",
    conditions = [],
    url = "https://spdx.org/licenses/OCCT-PL.html",
)

license_kind(
    name = "OCLC-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/OCLC-2.0.html",
)

license_kind(
    name = "ODbL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/ODbL-1.0.html",
)

license_kind(
    name = "ODC-By-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/ODC-By-1.0.html",
)

license_kind(
    name = "OFFIS",
    conditions = [],
    url = "https://spdx.org/licenses/OFFIS.html",
)

license_kind(
    name = "OFL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/OFL-1.0.html",
)

license_kind(
    name = "OFL-1.0-no-RFN",
    conditions = [],
    url = "https://spdx.org/licenses/OFL-1.0-no-RFN.html",
)

license_kind(
    name = "OFL-1.0-RFN",
    conditions = [],
    url = "https://spdx.org/licenses/OFL-1.0-RFN.html",
)

license_kind(
    name = "OFL-1.1",
    conditions = [],
    url = "https://spdx.org/licenses/OFL-1.1.html",
)

license_kind(
    name = "OFL-1.1-no-RFN",
    conditions = [],
    url = "https://spdx.org/licenses/OFL-1.1-no-RFN.html",
)

license_kind(
    name = "OFL-1.1-RFN",
    conditions = [],
    url = "https://spdx.org/licenses/OFL-1.1-RFN.html",
)

license_kind(
    name = "OGC-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/OGC-1.0.html",
)

license_kind(
    name = "OGDL-Taiwan-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/OGDL-Taiwan-1.0.html",
)

license_kind(
    name = "OGL-Canada-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/OGL-Canada-2.0.html",
)

license_kind(
    name = "OGL-UK-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/OGL-UK-1.0.html",
)

license_kind(
    name = "OGL-UK-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/OGL-UK-2.0.html",
)

license_kind(
    name = "OGL-UK-3.0",
    conditions = [],
    url = "https://spdx.org/licenses/OGL-UK-3.0.html",
)

license_kind(
    name = "OGTSL",
    conditions = [],
    url = "https://spdx.org/licenses/OGTSL.html",
)

license_kind(
    name = "OLDAP-1.1",
    conditions = [],
    url = "https://spdx.org/licenses/OLDAP-1.1.html",
)

license_kind(
    name = "OLDAP-1.2",
    conditions = [],
    url = "https://spdx.org/licenses/OLDAP-1.2.html",
)

license_kind(
    name = "OLDAP-1.3",
    conditions = [],
    url = "https://spdx.org/licenses/OLDAP-1.3.html",
)

license_kind(
    name = "OLDAP-1.4",
    conditions = [],
    url = "https://spdx.org/licenses/OLDAP-1.4.html",
)

license_kind(
    name = "OLDAP-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/OLDAP-2.0.html",
)

license_kind(
    name = "OLDAP-2.0.1",
    conditions = [],
    url = "https://spdx.org/licenses/OLDAP-2.0.1.html",
)

license_kind(
    name = "OLDAP-2.1",
    conditions = [],
    url = "https://spdx.org/licenses/OLDAP-2.1.html",
)

license_kind(
    name = "OLDAP-2.2",
    conditions = [],
    url = "https://spdx.org/licenses/OLDAP-2.2.html",
)

license_kind(
    name = "OLDAP-2.2.1",
    conditions = [],
    url = "https://spdx.org/licenses/OLDAP-2.2.1.html",
)

license_kind(
    name = "OLDAP-2.2.2",
    conditions = [],
    url = "https://spdx.org/licenses/OLDAP-2.2.2.html",
)

license_kind(
    name = "OLDAP-2.3",
    conditions = [],
    url = "https://spdx.org/licenses/OLDAP-2.3.html",
)

license_kind(
    name = "OLDAP-2.4",
    conditions = [],
    url = "https://spdx.org/licenses/OLDAP-2.4.html",
)

license_kind(
    name = "OLDAP-2.5",
    conditions = [],
    url = "https://spdx.org/licenses/OLDAP-2.5.html",
)

license_kind(
    name = "OLDAP-2.6",
    conditions = [],
    url = "https://spdx.org/licenses/OLDAP-2.6.html",
)

license_kind(
    name = "OLDAP-2.7",
    conditions = [],
    url = "https://spdx.org/licenses/OLDAP-2.7.html",
)

license_kind(
    name = "OLDAP-2.8",
    conditions = [],
    url = "https://spdx.org/licenses/OLDAP-2.8.html",
)

license_kind(
    name = "OLFL-1.3",
    conditions = [],
    url = "https://spdx.org/licenses/OLFL-1.3.html",
)

license_kind(
    name = "OML",
    conditions = [],
    url = "https://spdx.org/licenses/OML.html",
)

license_kind(
    name = "OpenPBS-2.3",
    conditions = [],
    url = "https://spdx.org/licenses/OpenPBS-2.3.html",
)

license_kind(
    name = "OpenSSL",
    conditions = [],
    url = "https://spdx.org/licenses/OpenSSL.html",
)

license_kind(
    name = "OpenSSL-standalone",
    conditions = [],
    url = "https://spdx.org/licenses/OpenSSL-standalone.html",
)

license_kind(
    name = "OpenVision",
    conditions = [],
    url = "https://spdx.org/licenses/OpenVision.html",
)

license_kind(
    name = "OPL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/OPL-1.0.html",
)

license_kind(
    name = "OPL-UK-3.0",
    conditions = [],
    url = "https://spdx.org/licenses/OPL-UK-3.0.html",
)

license_kind(
    name = "OPUBL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/OPUBL-1.0.html",
)

license_kind(
    name = "OSET-PL-2.1",
    conditions = [],
    url = "https://spdx.org/licenses/OSET-PL-2.1.html",
)

license_kind(
    name = "OSL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/OSL-1.0.html",
)

license_kind(
    name = "OSL-1.1",
    conditions = [],
    url = "https://spdx.org/licenses/OSL-1.1.html",
)

license_kind(
    name = "OSL-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/OSL-2.0.html",
)

license_kind(
    name = "OSL-2.1",
    conditions = [],
    url = "https://spdx.org/licenses/OSL-2.1.html",
)

license_kind(
    name = "OSL-3.0",
    conditions = [],
    url = "https://spdx.org/licenses/OSL-3.0.html",
)

license_kind(
    name = "PADL",
    conditions = [],
    url = "https://spdx.org/licenses/PADL.html",
)

license_kind(
    name = "Parity-6.0.0",
    conditions = [],
    url = "https://spdx.org/licenses/Parity-6.0.0.html",
)

license_kind(
    name = "Parity-7.0.0",
    conditions = [],
    url = "https://spdx.org/licenses/Parity-7.0.0.html",
)

license_kind(
    name = "PDDL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/PDDL-1.0.html",
)

license_kind(
    name = "PHP-3.0",
    conditions = [],
    url = "https://spdx.org/licenses/PHP-3.0.html",
)

license_kind(
    name = "PHP-3.01",
    conditions = [],
    url = "https://spdx.org/licenses/PHP-3.01.html",
)

license_kind(
    name = "Pixar",
    conditions = [],
    url = "https://spdx.org/licenses/Pixar.html",
)

license_kind(
    name = "pkgconf",
    conditions = [],
    url = "https://spdx.org/licenses/pkgconf.html",
)

license_kind(
    name = "Plexus",
    conditions = [],
    url = "https://spdx.org/licenses/Plexus.html",
)

license_kind(
    name = "pnmstitch",
    conditions = [],
    url = "https://spdx.org/licenses/pnmstitch.html",
)

license_kind(
    name = "PolyForm-Noncommercial-1.0.0",
    conditions = [],
    url = "https://spdx.org/licenses/PolyForm-Noncommercial-1.0.0.html",
)

license_kind(
    name = "PolyForm-Small-Business-1.0.0",
    conditions = [],
    url = "https://spdx.org/licenses/PolyForm-Small-Business-1.0.0.html",
)

license_kind(
    name = "PostgreSQL",
    conditions = [],
    url = "https://spdx.org/licenses/PostgreSQL.html",
)

license_kind(
    name = "PPL",
    conditions = [],
    url = "https://spdx.org/licenses/PPL.html",
)

license_kind(
    name = "PSF-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/PSF-2.0.html",
)

license_kind(
    name = "psfrag",
    conditions = [],
    url = "https://spdx.org/licenses/psfrag.html",
)

license_kind(
    name = "psutils",
    conditions = [],
    url = "https://spdx.org/licenses/psutils.html",
)

license_kind(
    name = "Python-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/Python-2.0.html",
)

license_kind(
    name = "Python-2.0.1",
    conditions = [],
    url = "https://spdx.org/licenses/Python-2.0.1.html",
)

license_kind(
    name = "python-ldap",
    conditions = [],
    url = "https://spdx.org/licenses/python-ldap.html",
)

license_kind(
    name = "Qhull",
    conditions = [],
    url = "https://spdx.org/licenses/Qhull.html",
)

license_kind(
    name = "QPL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/QPL-1.0.html",
)

license_kind(
    name = "QPL-1.0-INRIA-2004",
    conditions = [],
    url = "https://spdx.org/licenses/QPL-1.0-INRIA-2004.html",
)

license_kind(
    name = "radvd",
    conditions = [],
    url = "https://spdx.org/licenses/radvd.html",
)

license_kind(
    name = "Rdisc",
    conditions = [],
    url = "https://spdx.org/licenses/Rdisc.html",
)

license_kind(
    name = "RHeCos-1.1",
    conditions = [],
    url = "https://spdx.org/licenses/RHeCos-1.1.html",
)

license_kind(
    name = "RPL-1.1",
    conditions = [],
    url = "https://spdx.org/licenses/RPL-1.1.html",
)

license_kind(
    name = "RPL-1.5",
    conditions = [],
    url = "https://spdx.org/licenses/RPL-1.5.html",
)

license_kind(
    name = "RPSL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/RPSL-1.0.html",
)

license_kind(
    name = "RSA-MD",
    conditions = [],
    url = "https://spdx.org/licenses/RSA-MD.html",
)

license_kind(
    name = "RSCPL",
    conditions = [],
    url = "https://spdx.org/licenses/RSCPL.html",
)

license_kind(
    name = "Ruby",
    conditions = [],
    url = "https://spdx.org/licenses/Ruby.html",
)

license_kind(
    name = "Ruby-pty",
    conditions = [],
    url = "https://spdx.org/licenses/Ruby-pty.html",
)

license_kind(
    name = "SAX-PD",
    conditions = [],
    url = "https://spdx.org/licenses/SAX-PD.html",
)

license_kind(
    name = "SAX-PD-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/SAX-PD-2.0.html",
)

license_kind(
    name = "Saxpath",
    conditions = [],
    url = "https://spdx.org/licenses/Saxpath.html",
)

license_kind(
    name = "SCEA",
    conditions = [],
    url = "https://spdx.org/licenses/SCEA.html",
)

license_kind(
    name = "SchemeReport",
    conditions = [],
    url = "https://spdx.org/licenses/SchemeReport.html",
)

license_kind(
    name = "Sendmail",
    conditions = [],
    url = "https://spdx.org/licenses/Sendmail.html",
)

license_kind(
    name = "Sendmail-8.23",
    conditions = [],
    url = "https://spdx.org/licenses/Sendmail-8.23.html",
)

license_kind(
    name = "SGI-B-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/SGI-B-1.0.html",
)

license_kind(
    name = "SGI-B-1.1",
    conditions = [],
    url = "https://spdx.org/licenses/SGI-B-1.1.html",
)

license_kind(
    name = "SGI-B-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/SGI-B-2.0.html",
)

license_kind(
    name = "SGI-OpenGL",
    conditions = [],
    url = "https://spdx.org/licenses/SGI-OpenGL.html",
)

license_kind(
    name = "SGP4",
    conditions = [],
    url = "https://spdx.org/licenses/SGP4.html",
)

license_kind(
    name = "SHL-0.5",
    conditions = [],
    url = "https://spdx.org/licenses/SHL-0.5.html",
)

license_kind(
    name = "SHL-0.51",
    conditions = [],
    url = "https://spdx.org/licenses/SHL-0.51.html",
)

license_kind(
    name = "SimPL-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/SimPL-2.0.html",
)

license_kind(
    name = "SISSL",
    conditions = [],
    url = "https://spdx.org/licenses/SISSL.html",
)

license_kind(
    name = "SISSL-1.2",
    conditions = [],
    url = "https://spdx.org/licenses/SISSL-1.2.html",
)

license_kind(
    name = "SL",
    conditions = [],
    url = "https://spdx.org/licenses/SL.html",
)

license_kind(
    name = "Sleepycat",
    conditions = [],
    url = "https://spdx.org/licenses/Sleepycat.html",
)

license_kind(
    name = "SMLNJ",
    conditions = [],
    url = "https://spdx.org/licenses/SMLNJ.html",
)

license_kind(
    name = "SMPPL",
    conditions = [],
    url = "https://spdx.org/licenses/SMPPL.html",
)

license_kind(
    name = "SNIA",
    conditions = [],
    url = "https://spdx.org/licenses/SNIA.html",
)

license_kind(
    name = "snprintf",
    conditions = [],
    url = "https://spdx.org/licenses/snprintf.html",
)

license_kind(
    name = "softSurfer",
    conditions = [],
    url = "https://spdx.org/licenses/softSurfer.html",
)

license_kind(
    name = "Soundex",
    conditions = [],
    url = "https://spdx.org/licenses/Soundex.html",
)

license_kind(
    name = "Spencer-86",
    conditions = [],
    url = "https://spdx.org/licenses/Spencer-86.html",
)

license_kind(
    name = "Spencer-94",
    conditions = [],
    url = "https://spdx.org/licenses/Spencer-94.html",
)

license_kind(
    name = "Spencer-99",
    conditions = [],
    url = "https://spdx.org/licenses/Spencer-99.html",
)

license_kind(
    name = "SPL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/SPL-1.0.html",
)

license_kind(
    name = "ssh-keyscan",
    conditions = [],
    url = "https://spdx.org/licenses/ssh-keyscan.html",
)

license_kind(
    name = "SSH-OpenSSH",
    conditions = [],
    url = "https://spdx.org/licenses/SSH-OpenSSH.html",
)

license_kind(
    name = "SSH-short",
    conditions = [],
    url = "https://spdx.org/licenses/SSH-short.html",
)

license_kind(
    name = "SSLeay-standalone",
    conditions = [],
    url = "https://spdx.org/licenses/SSLeay-standalone.html",
)

license_kind(
    name = "SSPL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/SSPL-1.0.html",
)

license_kind(
    name = "StandardML-NJ",
    conditions = [],
    url = "https://spdx.org/licenses/StandardML-NJ.html",
)

license_kind(
    name = "SugarCRM-1.1.3",
    conditions = [],
    url = "https://spdx.org/licenses/SugarCRM-1.1.3.html",
)

license_kind(
    name = "Sun-PPP",
    conditions = [],
    url = "https://spdx.org/licenses/Sun-PPP.html",
)

license_kind(
    name = "Sun-PPP-2000",
    conditions = [],
    url = "https://spdx.org/licenses/Sun-PPP-2000.html",
)

license_kind(
    name = "SunPro",
    conditions = [],
    url = "https://spdx.org/licenses/SunPro.html",
)

license_kind(
    name = "SWL",
    conditions = [],
    url = "https://spdx.org/licenses/SWL.html",
)

license_kind(
    name = "swrule",
    conditions = [],
    url = "https://spdx.org/licenses/swrule.html",
)

license_kind(
    name = "Symlinks",
    conditions = [],
    url = "https://spdx.org/licenses/Symlinks.html",
)

license_kind(
    name = "TAPR-OHL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/TAPR-OHL-1.0.html",
)

license_kind(
    name = "TCL",
    conditions = [],
    url = "https://spdx.org/licenses/TCL.html",
)

license_kind(
    name = "TCP-wrappers",
    conditions = [],
    url = "https://spdx.org/licenses/TCP-wrappers.html",
)

license_kind(
    name = "TermReadKey",
    conditions = [],
    url = "https://spdx.org/licenses/TermReadKey.html",
)

license_kind(
    name = "TGPPL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/TGPPL-1.0.html",
)

license_kind(
    name = "threeparttable",
    conditions = [],
    url = "https://spdx.org/licenses/threeparttable.html",
)

license_kind(
    name = "TMate",
    conditions = [],
    url = "https://spdx.org/licenses/TMate.html",
)

license_kind(
    name = "TORQUE-1.1",
    conditions = [],
    url = "https://spdx.org/licenses/TORQUE-1.1.html",
)

license_kind(
    name = "TOSL",
    conditions = [],
    url = "https://spdx.org/licenses/TOSL.html",
)

license_kind(
    name = "TPDL",
    conditions = [],
    url = "https://spdx.org/licenses/TPDL.html",
)

license_kind(
    name = "TPL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/TPL-1.0.html",
)

license_kind(
    name = "TrustedQSL",
    conditions = [],
    url = "https://spdx.org/licenses/TrustedQSL.html",
)

license_kind(
    name = "TTWL",
    conditions = [],
    url = "https://spdx.org/licenses/TTWL.html",
)

license_kind(
    name = "TTYP0",
    conditions = [],
    url = "https://spdx.org/licenses/TTYP0.html",
)

license_kind(
    name = "TU-Berlin-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/TU-Berlin-1.0.html",
)

license_kind(
    name = "TU-Berlin-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/TU-Berlin-2.0.html",
)

license_kind(
    name = "Ubuntu-font-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/Ubuntu-font-1.0.html",
)

license_kind(
    name = "UCAR",
    conditions = [],
    url = "https://spdx.org/licenses/UCAR.html",
)

license_kind(
    name = "UCL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/UCL-1.0.html",
)

license_kind(
    name = "ulem",
    conditions = [],
    url = "https://spdx.org/licenses/ulem.html",
)

license_kind(
    name = "UMich-Merit",
    conditions = [],
    url = "https://spdx.org/licenses/UMich-Merit.html",
)

license_kind(
    name = "Unicode-3.0",
    conditions = [],
    url = "https://spdx.org/licenses/Unicode-3.0.html",
)

license_kind(
    name = "Unicode-DFS-2015",
    conditions = [],
    url = "https://spdx.org/licenses/Unicode-DFS-2015.html",
)

license_kind(
    name = "Unicode-DFS-2016",
    conditions = [],
    url = "https://spdx.org/licenses/Unicode-DFS-2016.html",
)

license_kind(
    name = "Unicode-TOU",
    conditions = [],
    url = "https://spdx.org/licenses/Unicode-TOU.html",
)

license_kind(
    name = "UnixCrypt",
    conditions = [],
    url = "https://spdx.org/licenses/UnixCrypt.html",
)

license_kind(
    name = "Unlicense",
    conditions = [],
    url = "https://spdx.org/licenses/Unlicense.html",
)

license_kind(
    name = "UPL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/UPL-1.0.html",
)

license_kind(
    name = "URT-RLE",
    conditions = [],
    url = "https://spdx.org/licenses/URT-RLE.html",
)

license_kind(
    name = "Vim",
    conditions = [],
    url = "https://spdx.org/licenses/Vim.html",
)

license_kind(
    name = "VOSTROM",
    conditions = [],
    url = "https://spdx.org/licenses/VOSTROM.html",
)

license_kind(
    name = "VSL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/VSL-1.0.html",
)

license_kind(
    name = "W3C",
    conditions = [],
    url = "https://spdx.org/licenses/W3C.html",
)

license_kind(
    name = "W3C-19980720",
    conditions = [],
    url = "https://spdx.org/licenses/W3C-19980720.html",
)

license_kind(
    name = "W3C-20150513",
    conditions = [],
    url = "https://spdx.org/licenses/W3C-20150513.html",
)

license_kind(
    name = "w3m",
    conditions = [],
    url = "https://spdx.org/licenses/w3m.html",
)

license_kind(
    name = "Watcom-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/Watcom-1.0.html",
)

license_kind(
    name = "Widget-Workshop",
    conditions = [],
    url = "https://spdx.org/licenses/Widget-Workshop.html",
)

license_kind(
    name = "Wsuipa",
    conditions = [],
    url = "https://spdx.org/licenses/Wsuipa.html",
)

license_kind(
    name = "WTFPL",
    conditions = [],
    url = "https://spdx.org/licenses/WTFPL.html",
)

license_kind(
    name = "wxWindows",
    conditions = [],
    url = "https://spdx.org/licenses/wxWindows.html",
)

license_kind(
    name = "X11",
    conditions = [],
    url = "https://spdx.org/licenses/X11.html",
)

license_kind(
    name = "X11-distribute-modifications-variant",
    conditions = [],
    url = "https://spdx.org/licenses/X11-distribute-modifications-variant.html",
)

license_kind(
    name = "X11-swapped",
    conditions = [],
    url = "https://spdx.org/licenses/X11-swapped.html",
)

license_kind(
    name = "Xdebug-1.03",
    conditions = [],
    url = "https://spdx.org/licenses/Xdebug-1.03.html",
)

license_kind(
    name = "Xerox",
    conditions = [],
    url = "https://spdx.org/licenses/Xerox.html",
)

license_kind(
    name = "Xfig",
    conditions = [],
    url = "https://spdx.org/licenses/Xfig.html",
)

license_kind(
    name = "XFree86-1.1",
    conditions = [],
    url = "https://spdx.org/licenses/XFree86-1.1.html",
)

license_kind(
    name = "xinetd",
    conditions = [],
    url = "https://spdx.org/licenses/xinetd.html",
)

license_kind(
    name = "xkeyboard-config-Zinoviev",
    conditions = [],
    url = "https://spdx.org/licenses/xkeyboard-config-Zinoviev.html",
)

license_kind(
    name = "xlock",
    conditions = [],
    url = "https://spdx.org/licenses/xlock.html",
)

license_kind(
    name = "Xnet",
    conditions = [],
    url = "https://spdx.org/licenses/Xnet.html",
)

license_kind(
    name = "xpp",
    conditions = [],
    url = "https://spdx.org/licenses/xpp.html",
)

license_kind(
    name = "XSkat",
    conditions = [],
    url = "https://spdx.org/licenses/XSkat.html",
)

license_kind(
    name = "xzoom",
    conditions = [],
    url = "https://spdx.org/licenses/xzoom.html",
)

license_kind(
    name = "YPL-1.0",
    conditions = [],
    url = "https://spdx.org/licenses/YPL-1.0.html",
)

license_kind(
    name = "YPL-1.1",
    conditions = [],
    url = "https://spdx.org/licenses/YPL-1.1.html",
)

license_kind(
    name = "Zed",
    conditions = [],
    url = "https://spdx.org/licenses/Zed.html",
)

license_kind(
    name = "Zeeff",
    conditions = [],
    url = "https://spdx.org/licenses/Zeeff.html",
)

license_kind(
    name = "Zend-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/Zend-2.0.html",
)

license_kind(
    name = "Zimbra-1.3",
    conditions = [],
    url = "https://spdx.org/licenses/Zimbra-1.3.html",
)

license_kind(
    name = "Zimbra-1.4",
    conditions = [],
    url = "https://spdx.org/licenses/Zimbra-1.4.html",
)

license_kind(
    name = "Zlib",
    conditions = [],
    url = "https://spdx.org/licenses/Zlib.html",
)

license_kind(
    name = "zlib-acknowledgement",
    conditions = [],
    url = "https://spdx.org/licenses/zlib-acknowledgement.html",
)

license_kind(
    name = "ZPL-1.1",
    conditions = [],
    url = "https://spdx.org/licenses/ZPL-1.1.html",
)

license_kind(
    name = "ZPL-2.0",
    conditions = [],
    url = "https://spdx.org/licenses/ZPL-2.0.html",
)

license_kind(
    name = "ZPL-2.1",
    conditions = [],
    url = "https://spdx.org/licenses/ZPL-2.1.html",
)
