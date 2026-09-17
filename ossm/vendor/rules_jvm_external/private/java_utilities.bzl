#
# Utilites for working with java command line
#

# Build the contents of a java Command-Line Argument File from a list of
# arguments.
#
# This quotes all arguments (and escapes all quotation marks in arguments) so
# that arguments containing white space are treated as single arguments.
def build_java_argsfile_content(args):
    return "\n".join(['"' + str(f).replace('"', r'\"') + '"' for f in args]) + "\n"
