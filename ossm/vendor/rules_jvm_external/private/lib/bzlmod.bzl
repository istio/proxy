# Utility functions for handling bzlmod-related features

def get_module_name_of_owner_of_repo(name):
    """Returns the name of the module that owns the repo that owns `name` (typically from `File.owner.workspace_name`).

    Args:
      name: (string) The name to convert, often this will be from `File.owner.workspace_name`
    """

    # Common case first
    if "+" in name:
        return name.partition("+")[0]

    return name.partition("~")[0]
