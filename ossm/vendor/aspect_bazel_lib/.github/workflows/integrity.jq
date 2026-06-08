# JQ filter to transform sha256 files to a value we can read from starlark.
# NB: the sha256 files are expected to be newline-terminated.
#
# Input looks like
# 48552e399a1f2ab97e62ca7fce5783b6214e284330c7555383f43acf82446636 unpack-linux-aarch64\nfd265552bfd236efef519f81ce783322a50d8d7ab5af5d08a713e519cedff87f unpack-linux-x86_64\n
#
# Output should look like
# {
#  "unpack-linux-aarch64": "48552e399a1f2ab97e62ca7fce5783b6214e284330c7555383f43acf82446636",
#  "unpack-linux-x86_64": "fd265552bfd236efef519f81ce783322a50d8d7ab5af5d08a713e519cedff87f"
# }

.
# Don't end with an empty object
| rtrimstr("\n")
| split("\n")
| map(
    split(" ")
    | {"key": .[1], "value": .[0]}
  )
| from_entries
