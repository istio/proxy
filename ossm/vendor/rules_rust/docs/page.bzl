"""A helper module for generating documentation for rules_rust"""

def page(name, symbols, header_template = None):
    """Define a collection of attributes used to generate a page of documentation

    Note, all templates are Velocity files: https://velocity.apache.org/engine/devel/user-guide.html

    Args:
        name (str): The name of the page
        symbols (list): A list of symbol names
        header_template (Label, optional): The label of a `header_template` stardoc attribute

    Returns:
        tuple: The name of the page with the page content
    """
    return (name, struct(
        name = name,
        header_template = header_template,
        symbols = symbols,
    ))

# buildifier: disable=unnamed-macro
def gen_header(page):
    """Generate a header with a table of contents

    Args:
        page (struct): A `page` struct
    """
    name = "%s_gen_header_vm" % page.name
    outs = ["%s_gen_header.vm" % page.name]

    # Set the top level header
    page_names = [w.capitalize() for w in page.name.split("_")]
    cmd = [
        "echo '<!-- Generated with Stardoc: http://skydoc.bazel.build -->'",
        "echo '# {}'".format(" ".join(page_names)),
        "echo ''",
    ]

    # Add table of contents
    cmd.extend(["echo '* [{rule}](#{rule})'".format(rule = s) for s in page.symbols])

    # Render an optional header
    if page.header_template:
        cmd.extend([
            "echo ''",
            "cat $(execpath {})".format(page.header_template),
        ])
        srcs = [page.header_template]
    else:
        srcs = []

    native.genrule(
        name = name,
        outs = outs,
        cmd = "{\n" + "\n".join(cmd) + "\n} > $@",
        srcs = srcs,
        output_to_bindir = True,
    )
