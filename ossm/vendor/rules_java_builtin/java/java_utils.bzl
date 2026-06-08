"""Utility methods for interacting with the java rules"""

def _tokenize_javacopts(ctx, opts):
    """Tokenizes a list or depset of options to a list.

    Iff opts is a depset, we reverse the flattened list to ensure right-most
    duplicates are preserved in their correct position.

    Args:
        ctx: (RuleContext) the rule context
        opts: (depset[str]|[str]) the javac options to tokenize
    Returns:
        [str] list of tokenized options
    """
    if hasattr(opts, "to_list"):
        opts = reversed(opts.to_list())
    return [
        token
        for opt in opts
        for token in ctx.tokenize(opt)
    ]

utils = struct(
    tokenize_javacopts = _tokenize_javacopts,
)
