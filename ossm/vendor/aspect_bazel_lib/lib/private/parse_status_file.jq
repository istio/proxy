[
  split("\n")[]                                     # Convert lines to array
  | capture("(?<key>[^\\s]+)\\s+(?<value>.*)"; "x") # capture {"key": [everything before first whitespace], "value": [remainder of line]}
]
| from_entries  # Convert [{"key": "a", "value": "b"}] to map {"a": "b"}
