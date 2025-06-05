# Copyright 2023 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import argparse
import io
import itertools
import pathlib
import sys
import textwrap
from typing import Callable, TextIO, TypeVar

from stardoc.proto import stardoc_output_pb2

_AttributeType = stardoc_output_pb2.AttributeType

_T = TypeVar("_T")


def _anchor_id(text: str) -> str:
    # MyST/Sphinx's markdown processing doesn't like dots in anchor ids.
    return "#" + text.replace(".", "_").lower()


# Create block attribute line.
# See https://myst-parser.readthedocs.io/en/latest/syntax/optional.html#block-attributes
def _block_attrs(*attrs: str) -> str:
    return "{" + " ".join(attrs) + "}\n"


def _link(display: str, link: str = "", *, ref: str = "", classes: str = "") -> str:
    if ref:
        ref = f"[{ref}]"
    if link:
        link = f"({link})"
    if classes:
        classes = "{" + classes + "}"
    return f"[{display}]{ref}{link}{classes}"


def _span(display: str, classes: str = ".span") -> str:
    return f"[{display}]{{" + classes + "}"


def _link_here_icon(anchor: str) -> str:
    # The headerlink class activates some special logic to show/hide
    # text upon mouse-over; it's how headings show a clickable link.
    return _link("¶", anchor, classes=".headerlink")


def _inline_anchor(anchor: str) -> str:
    return _span("", anchor)


def _indent_block_text(text: str) -> str:
    return text.strip().replace("\n", "\n  ")


def _join_csv_and(values: list[str]) -> str:
    if len(values) == 1:
        return values[0]

    values = list(values)
    values[-1] = "and " + values[-1]
    return ", ".join(values)


def _position_iter(values: list[_T]) -> tuple[bool, bool, _T]:
    for i, value in enumerate(values):
        yield i == 0, i == len(values) - 1, value


def _sort_attributes_inplace(attributes):
    # Sort attributes so the iteration order results in a Python-syntax
    # valid signature. Keep name first because that's convention.
    attributes.sort(key=lambda a: (a.name != "name", bool(a.default_value), a.name))


class _MySTRenderer:
    def __init__(
        self,
        module: stardoc_output_pb2.ModuleInfo,
        out_stream: TextIO,
        public_load_path: str,
    ):
        self._module = module
        self._out_stream = out_stream
        self._public_load_path = public_load_path
        self._typedef_stack = []

    def _get_colons(self):
        # There's a weird behavior where increasing colon indents doesn't
        # parse as nested objects correctly, so we have to reduce the
        # number of colons based on the indent level
        indent = 10 - len(self._typedef_stack)
        assert indent >= 0
        return ":::" + ":" * indent

    def render(self):
        self._render_module(self._module)

    def _render_module(self, module: stardoc_output_pb2.ModuleInfo):
        if self._public_load_path:
            bzl_path = self._public_load_path
        else:
            bzl_path = "//" + self._module.file.split("//")[1]

        self._write(":::{default-domain} bzl\n:::\n")
        self._write(":::{bzl:currentfile} ", bzl_path, "\n:::\n\n")
        self._write(
            f"# {bzl_path}\n",
            "\n",
            module.module_docstring.strip(),
            "\n\n",
        )

        objects = itertools.chain(
            ((r.rule_name, r, self._render_rule) for r in module.rule_info),
            ((p.provider_name, p, self._render_provider) for p in module.provider_info),
            ((f.function_name, f, self._process_func_info) for f in module.func_info),
            ((a.aspect_name, a, self._render_aspect) for a in module.aspect_info),
            (
                (m.extension_name, m, self._render_module_extension)
                for m in module.module_extension_info
            ),
            (
                (r.rule_name, r, self._render_repository_rule)
                for r in module.repository_rule_info
            ),
        )
        # Sort by name, ignoring case. The `.TYPEDEF` string is removed so
        # that the .TYPEDEF entries come before what is in the typedef.
        objects = sorted(objects, key=lambda v: v[0].removesuffix(".TYPEDEF").lower())

        for name, obj, func in objects:
            self._process_object(name, obj, func)
            self._write("\n")

        # Close any typedefs
        while self._typedef_stack:
            self._typedef_stack.pop()
            self._render_typedef_end()

    def _process_object(self, name, obj, renderer):
        # The trailing doc is added to prevent matching a common prefix
        typedef_group = name.removesuffix(".TYPEDEF") + "."
        while self._typedef_stack and not typedef_group.startswith(
            self._typedef_stack[-1]
        ):
            self._typedef_stack.pop()
            self._render_typedef_end()
        renderer(obj)
        if name.endswith(".TYPEDEF"):
            self._typedef_stack.append(typedef_group)

    def _render_aspect(self, aspect: stardoc_output_pb2.AspectInfo):
        _sort_attributes_inplace(aspect.attribute)
        self._write("::::::{bzl:aspect} ", aspect.aspect_name, "\n\n")
        edges = ", ".join(sorted(f"`{attr}`" for attr in aspect.aspect_attribute))
        self._write(":aspect-attributes: ", edges, "\n\n")
        self._write(aspect.doc_string.strip(), "\n\n")

        if aspect.attribute:
            self._render_attributes(aspect.attribute)
            self._write("\n")
        self._write("::::::\n")

    def _render_module_extension(self, mod_ext: stardoc_output_pb2.ModuleExtensionInfo):
        self._write("::::::{bzl:module-extension} ", mod_ext.extension_name, "\n\n")
        self._write(mod_ext.doc_string.strip(), "\n\n")

        for tag in mod_ext.tag_class:
            tag_name = f"{mod_ext.extension_name}.{tag.tag_name}"
            tag_name = f"{tag.tag_name}"
            self._write(":::::{bzl:tag-class} ")

            _sort_attributes_inplace(tag.attribute)
            self._render_signature(
                tag_name,
                tag.attribute,
                get_name=lambda a: a.name,
                get_default=lambda a: a.default_value,
            )

            if doc_string := tag.doc_string.strip():
                self._write(doc_string, "\n\n")
            # Ensure a newline between the directive and the doc fields,
            # otherwise they get parsed as directive options instead.
            if not doc_string and tag.attribute:
                self._write("\n")
            self._render_attributes(tag.attribute)
            self._write(":::::\n")
        self._write("::::::\n")

    def _render_repository_rule(self, repo_rule: stardoc_output_pb2.RepositoryRuleInfo):
        self._write("::::::{bzl:repo-rule} ")
        _sort_attributes_inplace(repo_rule.attribute)
        self._render_signature(
            repo_rule.rule_name,
            repo_rule.attribute,
            get_name=lambda a: a.name,
            get_default=lambda a: a.default_value,
        )
        self._write(repo_rule.doc_string.strip(), "\n\n")
        if repo_rule.attribute:
            self._render_attributes(repo_rule.attribute)
        if repo_rule.environ:
            self._write(":envvars: ", ", ".join(sorted(repo_rule.environ)))
        self._write("\n")

    def _render_rule(self, rule: stardoc_output_pb2.RuleInfo):
        rule_name = rule.rule_name
        _sort_attributes_inplace(rule.attribute)
        self._write("::::{bzl:rule} ")
        self._render_signature(
            rule_name,
            rule.attribute,
            get_name=lambda r: r.name,
            get_default=lambda r: r.default_value,
        )
        self._write(rule.doc_string.strip(), "\n\n")

        if rule.advertised_providers.provider_name:
            self._write(":provides: ")
            self._write(" | ".join(rule.advertised_providers.provider_name))
            self._write("\n")
        self._write("\n")

        if rule.attribute:
            self._render_attributes(rule.attribute)
            self._write("\n")
        self._write("::::\n")

    def _rule_attr_type_string(self, attr: stardoc_output_pb2.AttributeInfo) -> str:
        if attr.type == _AttributeType.NAME:
            return "Name"
        elif attr.type == _AttributeType.INT:
            return "int"
        elif attr.type == _AttributeType.LABEL:
            return "label"
        elif attr.type == _AttributeType.STRING:
            return "str"
        elif attr.type == _AttributeType.STRING_LIST:
            return "list[str]"
        elif attr.type == _AttributeType.INT_LIST:
            return "list[int]"
        elif attr.type == _AttributeType.LABEL_LIST:
            return "list[label]"
        elif attr.type == _AttributeType.BOOLEAN:
            return "bool"
        elif attr.type == _AttributeType.LABEL_STRING_DICT:
            return "dict[label, str]"
        elif attr.type == _AttributeType.STRING_DICT:
            return "dict[str, str]"
        elif attr.type == _AttributeType.STRING_LIST_DICT:
            return "dict[str, list[str]]"
        elif attr.type == _AttributeType.OUTPUT:
            return "label"
        elif attr.type == _AttributeType.OUTPUT_LIST:
            return "list[label]"
        else:
            # If we get here, it means the value was unknown for some reason.
            # Rather than error, give some somewhat understandable value.
            return _AttributeType.Name(attr.type)

    def _process_func_info(self, func):
        if func.function_name.endswith(".TYPEDEF"):
            self._render_typedef_start(func)
        else:
            self._render_func(func)

    def _render_typedef_start(self, func):
        self._write(
            self._get_colons(),
            "{bzl:typedef} ",
            func.function_name.removesuffix(".TYPEDEF"),
            "\n",
        )
        if func.doc_string:
            self._write(func.doc_string.strip(), "\n")

    def _render_typedef_end(self):
        self._write(self._get_colons(), "\n\n")

    def _render_func(self, func: stardoc_output_pb2.StarlarkFunctionInfo):
        self._write(self._get_colons(), "{bzl:function} ")

        parameters = self._render_func_signature(func)

        doc_string = func.doc_string.strip()
        if doc_string:
            self._write(doc_string, "\n\n")

        if parameters:
            # Ensure a newline between the directive and the doc fields,
            # otherwise they get parsed as directive options instead.
            if not doc_string:
                self._write("\n")
            for param in parameters:
                self._write(f":arg {param.name}:\n")
                if param.default_value:
                    default_value = self._format_default_value(param.default_value)
                    self._write("  {default-value}`", default_value, "`\n")
                if param.doc_string:
                    self._write("  ", _indent_block_text(param.doc_string), "\n")
                else:
                    self._write("  _undocumented_\n")
                self._write("\n")

        if return_doc := getattr(func, "return").doc_string:
            self._write(":returns:\n")
            self._write("  ", _indent_block_text(return_doc), "\n")
        if func.deprecated.doc_string:
            self._write(":::::{deprecated}: unknown\n")
            self._write("  ", _indent_block_text(func.deprecated.doc_string), "\n")
            self._write(":::::\n")
        self._write(self._get_colons(), "\n")

    def _render_func_signature(self, func):
        func_name = func.function_name
        if self._typedef_stack:
            func_name = func.function_name.removeprefix(self._typedef_stack[-1])
        self._write(f"{func_name}(")
        # TODO: Have an "is method" directive in the docstring to decide if
        # the self parameter should be removed.
        parameters = [param for param in func.parameter if param.name != "self"]

        # Unfortunately, the stardoc info is incomplete and inaccurate:
        # * The position of the `*args` param is wrong; it'll always
        #   be last (or second to last, if kwargs is present).
        # * Stardoc doesn't explicitly tell us if an arg is `*args` or
        #   `**kwargs`. Hence f(*args) or f(**kwargs) is ambigiguous.
        # See these issues:
        # https://github.com/bazelbuild/stardoc/issues/226
        # https://github.com/bazelbuild/stardoc/issues/225
        #
        # Below, we try to take what info we have and infer what the original
        # signature was. In short:
        # * A default=empty, mandatory=false arg is either *args or **kwargs
        # * If two of those are seen, the first is *args and the second is
        #   **kwargs. Recall, however, the position of *args is mis-represented.
        # * If a single default=empty, mandatory=false arg is found, then
        #   it's ambiguous as to whether its *args or **kwargs. To figure
        #   that out, we:
        #   * If it's not the last arg, then it must be *args. In practice,
        #     this never occurs due to #226 above.
        #   * If we saw a mandatory arg after an optional arg, then *args
        #     was supposed to be between them (otherwise it wouldn't be
        #     valid syntax).
        #   * Otherwise, it's ambiguous. We just guess by looking at the
        #     parameter name.
        var_args = None
        var_kwargs = None
        saw_mandatory_after_optional = False
        first_mandatory_after_optional_index = None
        optionals_started = False
        for i, p in enumerate(parameters):
            optionals_started = optionals_started or not p.mandatory
            if p.mandatory and optionals_started:
                saw_mandatory_after_optional = True
                if first_mandatory_after_optional_index is None:
                    first_mandatory_after_optional_index = i

            if not p.default_value and not p.mandatory:
                if var_args is None:
                    var_args = (i, p)
                else:
                    var_kwargs = p

        if var_args and not var_kwargs:
            if var_args[0] != len(parameters) - 1:
                pass
            elif saw_mandatory_after_optional:
                var_kwargs = var_args[1]
                var_args = None
            elif var_args[1].name in ("kwargs", "attrs"):
                var_kwargs = var_args[1]
                var_args = None

        # Partial workaround for
        # https://github.com/bazelbuild/stardoc/issues/226: `*args` renders last
        if var_args and var_kwargs and first_mandatory_after_optional_index is not None:
            parameters.pop(var_args[0])
            parameters.insert(first_mandatory_after_optional_index, var_args[1])

        # The only way a mandatory-after-optional can occur is
        # if there was `*args` before it. But if we didn't see it,
        # it must have been the unbound `*` symbol, which stardoc doesn't
        # tell us exists.
        if saw_mandatory_after_optional and not var_args:
            self._write("*, ")
        for _, is_last, p in _position_iter(parameters):
            if var_args and p.name == var_args[1].name:
                self._write("*")
            elif var_kwargs and p.name == var_kwargs.name:
                self._write("**")
            self._write(p.name)
            if p.default_value:
                self._write("=", self._format_default_value(p.default_value))
            if not is_last:
                self._write(", ")
        self._write(")\n")
        return parameters

    def _render_provider(self, provider: stardoc_output_pb2.ProviderInfo):
        self._write("::::::{bzl:provider} ", provider.provider_name, "\n")
        if provider.origin_key:
            self._render_origin_key_option(provider.origin_key)
        self._write("\n")

        self._write(provider.doc_string.strip(), "\n\n")

        self._write(":::::{bzl:function} ")
        provider.field_info.sort(key=lambda f: f.name)
        self._render_signature(
            "<init>",
            provider.field_info,
            get_name=lambda f: f.name,
        )
        # TODO: Add support for provider.init once our Bazel version supports
        # that field
        self._write(":::::\n")

        for field in provider.field_info:
            self._write(":::::{bzl:provider-field} ", field.name, "\n")
            self._write(field.doc_string.strip())
            self._write("\n")
            self._write(":::::\n")
        self._write("::::::\n")

    def _render_attributes(self, attributes: list[stardoc_output_pb2.AttributeInfo]):
        for attr in attributes:
            attr_type = self._rule_attr_type_string(attr)
            self._write(f":attr {attr.name}:\n")
            if attr.default_value:
                self._write("  {bzl:default-value}`%s`\n" % attr.default_value)
            self._write("  {type}`%s`\n" % attr_type)
            self._write("  ", _indent_block_text(attr.doc_string), "\n")
            self._write("  :::{bzl:attr-info} Info\n")
            if attr.mandatory:
                self._write("  :mandatory:\n")
            self._write("  :::\n")
            self._write("\n")

            if attr.provider_name_group:
                self._write("  {required-providers}`")
                for _, outer_is_last, provider_group in _position_iter(
                    attr.provider_name_group
                ):
                    pairs = list(
                        zip(
                            provider_group.origin_key,
                            provider_group.provider_name,
                            strict=True,
                        )
                    )
                    if len(pairs) > 1:
                        self._write("[")
                    for _, inner_is_last, (origin_key, name) in _position_iter(pairs):
                        if origin_key.file == "<native>":
                            origin = origin_key.name
                        else:
                            origin = f"{origin_key.file}%{origin_key.name}"
                        # We have to use "title <ref>" syntax because the same
                        # name might map to different origins. Stardoc gives us
                        # the provider's actual name, not the name of the symbol
                        # used in the source.
                        self._write(f"'{name} <{origin}>'")
                        if not inner_is_last:
                            self._write(", ")

                    if len(pairs) > 1:
                        self._write("]")

                    if not outer_is_last:
                        self._write(" | ")
                self._write("`\n")

            self._write("\n")

    def _render_signature(
        self,
        name: str,
        parameters: list[_T],
        *,
        get_name: Callable[_T, str],
        get_default: Callable[_T, str] = lambda v: None,
    ):
        self._write(name, "(")
        for _, is_last, param in _position_iter(parameters):
            param_name = get_name(param)
            self._write(f"{param_name}")
            default_value = get_default(param)
            if default_value:
                default_value = self._format_default_value(default_value)
                self._write(f"={default_value}")
            if not is_last:
                self._write(", ")
        self._write(")\n\n")

    def _render_origin_key_option(self, origin_key, indent=""):
        self._write(
            indent,
            ":origin-key: ",
            self._format_option_value(f"{origin_key.file}%{origin_key.name}"),
            "\n",
        )

    def _format_default_value(self, default_value):
        # Handle <function foo from //baz:bar.bzl>
        # For now, just use quotes for lack of a better option
        if default_value.startswith("<"):
            return f"'{default_value}'"
        elif default_value.startswith("Label("):
            # Handle Label(*, "@some//label:target")
            start_quote = default_value.find('"')
            end_quote = default_value.rfind('"')
            return default_value[start_quote : end_quote + 1]
        else:
            return default_value

    def _format_option_value(self, value):
        # Leading @ symbols are special markup; escape them.
        if value.startswith("@"):
            return "\\" + value
        else:
            return value

    def _write(self, *lines: str):
        self._out_stream.writelines(lines)


def _convert(
    *,
    proto: pathlib.Path,
    output: pathlib.Path,
    public_load_path: str,
):
    module = stardoc_output_pb2.ModuleInfo.FromString(proto.read_bytes())
    with output.open("wt", encoding="utf8") as out_stream:
        _MySTRenderer(module, out_stream, public_load_path).render()


def _create_parser():
    parser = argparse.ArgumentParser(fromfile_prefix_chars="@")
    parser.add_argument("--proto", dest="proto", type=pathlib.Path)
    parser.add_argument("--output", dest="output", type=pathlib.Path)
    parser.add_argument("--public-load-path", dest="public_load_path")
    return parser


def main(args):
    options = _create_parser().parse_args(args)
    _convert(
        proto=options.proto,
        output=options.output,
        public_load_path=options.public_load_path,
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
