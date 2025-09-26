"Public API for expand template"

load("@bazel_skylib//lib:types.bzl", "types")
load("@bazel_skylib//rules:write_file.bzl", "write_file")
load("//lib/private:expand_template.bzl", _expand_template = "expand_template")

expand_template_rule = _expand_template

def expand_template(name, template, **kwargs):
    """Wrapper macro for `expand_template_rule`.

    Args:
        name: name of resulting rule
        template: the label of a template file, or a list of strings
            which are lines representing the content of the template.
        **kwargs: other named parameters to `expand_template_rule`.
    """
    if types.is_list(template):
        write_target = "{}_tmpl".format(name)
        write_file(
            name = write_target,
            out = "{}.txt".format(write_target),
            content = template,
        )
        template = write_target

    _expand_template(
        name = name,
        template = template,
        **kwargs
    )
