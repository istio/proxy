"""Declares provider `LicenseKindInfo`."""

visibility("public")

def _init(identifier, name):
    return {
        "identifier": identifier,
        "name": name,
    }

LicenseKindInfo, _create = provider(
    doc = """
Provides information to identify a license.
""".strip(),
    fields = {
        "identifier": """
A [string](https://bazel.build/rules/lib/core/string) uniquely identifying the
license (e.g., `Apache-2.0`, `EUPL-1.1`).

This is typically the [SPDX identifier](https://spdx.org/licenses/) of the
license, but may also be a non-standard value (e.g., in case of a commercial
license).
""".strip(),
        "name": """
A [string](https://bazel.build/rules/lib/core/string) containing the (human
readable) name of the license (e.g., `Apache License 2.0`, `European Union
Public License 1.1`)
""".strip(),
    },
    init = _init,
)
