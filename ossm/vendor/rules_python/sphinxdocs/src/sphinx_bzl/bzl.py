# Copyright 2024 The Bazel Authors. All rights reserved.
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
"""Sphinx extension for documenting Bazel/Starlark objects."""

import ast
import collections
import enum
import os
import typing
from collections.abc import Collection
from typing import Callable, Iterable, TypeVar

from docutils import nodes as docutils_nodes
from docutils.parsers.rst import directives as docutils_directives
from docutils.parsers.rst import states
from sphinx import addnodes, builders
from sphinx import directives as sphinx_directives
from sphinx import domains, environment, roles
from sphinx.highlighting import lexer_classes
from sphinx.locale import _
from sphinx.util import docfields
from sphinx.util import docutils as sphinx_docutils
from sphinx.util import inspect, logging
from sphinx.util import nodes as sphinx_nodes
from sphinx.util import typing as sphinx_typing
from typing_extensions import TypeAlias, override

_logger = logging.getLogger(__name__)
_LOG_PREFIX = f"[{_logger.name}] "

_INDEX_SUBTYPE_NORMAL = 0
_INDEX_SUBTYPE_ENTRY_WITH_SUB_ENTRIES = 1
_INDEX_SUBTYPE_SUB_ENTRY = 2

_T = TypeVar("_T")

# See https://www.sphinx-doc.org/en/master/extdev/domainapi.html#sphinx.domains.Domain.get_objects
_GetObjectsTuple: TypeAlias = tuple[str, str, str, str, str, int]

# See SphinxRole.run definition; the docs for role classes are pretty sparse.
_RoleRunResult: TypeAlias = tuple[
    list[docutils_nodes.Node], list[docutils_nodes.system_message]
]


def _log_debug(message, *args):
    # NOTE: Non-warning log messages go to stdout and are only
    # visible when -q isn't passed to Sphinx. Note that the sphinx_docs build
    # rule passes -q by default; use --//sphinxdocs:quiet=false to disable it.
    _logger.debug("%s" + message, _LOG_PREFIX, *args)


def _position_iter(values: Collection[_T]) -> tuple[bool, bool, _T]:
    last_i = len(values) - 1
    for i, value in enumerate(values):
        yield i == 0, i == last_i, value


class InvalidValueError(Exception):
    """Generic error for an invalid value instead of ValueError.

    Sphinx treats regular ValueError to mean abort parsing the current
    chunk and continue on as best it can. Their error means a more
    fundamental problem that should cause a failure.
    """


class _ObjectEntry:
    """Metadata about a known object."""

    def __init__(
        self,
        full_id: str,
        display_name: str,
        object_type: str,
        search_priority: int,
        index_entry: domains.IndexEntry,
    ):
        """Creates an instance.

        Args:
            full_id: The fully qualified id of the object. Should be
                globally unique, even between projects.
            display_name: What to display the object as in casual context.
            object_type: The type of object, typically one of the values
                known to the domain.
            search_priority: The search priority, see
                https://www.sphinx-doc.org/en/master/extdev/domainapi.html#sphinx.domains.Domain.get_objects
                for valid values.
            index_entry: Metadata about the object for the domain index.
        """
        self.full_id = full_id
        self.display_name = display_name
        self.object_type = object_type
        self.search_priority = search_priority
        self.index_entry = index_entry

    def to_get_objects_tuple(self) -> _GetObjectsTuple:
        # For the tuple definition
        return (
            self.full_id,
            self.display_name,
            self.object_type,
            self.index_entry.docname,
            self.index_entry.anchor,
            self.search_priority,
        )

    def __repr__(self):
        return f"ObjectEntry({self.full_id=}, {self.object_type=}, {self.display_name=}, {self.index_entry.docname=})"


# A simple helper just to document what the index tuple nodes are.
def _index_node_tuple(
    entry_type: str,
    entry_name: str,
    target: str,
    main: typing.Union[str, None] = None,
    category_key: typing.Union[str, None] = None,
) -> tuple[str, str, str, typing.Union[str, None], typing.Union[str, None]]:
    # For this tuple definition, see:
    # https://www.sphinx-doc.org/en/master/extdev/nodes.html#sphinx.addnodes.index
    # For the definition of entry_type, see:
    # And https://www.sphinx-doc.org/en/master/usage/restructuredtext/directives.html#directive-index
    return (entry_type, entry_name, target, main, category_key)


class _BzlObjectId:
    """Identifies an object defined by a directive.

    This object is returned by `handle_signature()` and passed onto
    `add_target_and_index()`. It contains information to identify the object
    that is being described so that it can be indexed and tracked by the
    domain.
    """

    def __init__(
        self,
        *,
        repo: str,
        label: str,
        namespace: str = None,
        symbol: str = None,
    ):
        """Creates an instance.

        Args:
            repo: repository name, including leading "@".
            bzl_file: label of file containing the object, e.g. //foo:bar.bzl
            namespace: dotted name of the namespace the symbol is within.
            symbol: dotted name, relative to `namespace` of the symbol.
        """
        if not repo:
            raise InvalidValueError("repo cannot be empty")
        if not repo.startswith("@"):
            raise InvalidValueError("repo must start with @")
        if not label:
            raise InvalidValueError("label cannot be empty")
        if not label.startswith("//"):
            raise InvalidValueError("label must start with //")

        if not label.endswith(".bzl") and (symbol or namespace):
            raise InvalidValueError(
                "Symbol and namespace can only be specified for .bzl labels"
            )

        self.repo = repo
        self.label = label
        self.package, self.target_name = self.label.split(":")
        self.namespace = namespace
        self.symbol = symbol  # Relative to namespace
        # doc-relative identifier for this object
        self.doc_id = symbol or self.target_name

        if not self.doc_id:
            raise InvalidValueError("doc_id is empty")

        self.full_id = _full_id_from_parts(repo, label, [namespace, symbol])

    @classmethod
    def from_env(
        cls, env: environment.BuildEnvironment, *, symbol: str = None, label: str = None
    ) -> "_BzlObjectId":
        label = label or env.ref_context["bzl:file"]
        if symbol:
            namespace = ".".join(env.ref_context["bzl:doc_id_stack"])
        else:
            namespace = None

        return cls(
            repo=env.ref_context["bzl:repo"],
            label=label,
            namespace=namespace,
            symbol=symbol,
        )

    def __repr__(self):
        return f"_BzlObjectId({self.full_id=})"


def _full_id_from_env(env, object_ids=None):
    return _full_id_from_parts(
        env.ref_context["bzl:repo"],
        env.ref_context["bzl:file"],
        env.ref_context["bzl:object_id_stack"] + (object_ids or []),
    )


def _full_id_from_parts(repo, bzl_file, symbol_names=None):
    parts = [repo, bzl_file]

    symbol_names = symbol_names or []
    symbol_names = list(filter(None, symbol_names))  # Filter out empty values
    if symbol_names:
        parts.append("%")
        parts.append(".".join(symbol_names))

    full_id = "".join(parts)
    return full_id


def _parse_full_id(full_id):
    repo, slashes, label = full_id.partition("//")
    label = slashes + label
    label, _, symbol = label.partition("%")
    return (repo, label, symbol)


class _TypeExprParser(ast.NodeVisitor):
    """Parsers a string description of types to doc nodes."""

    def __init__(self, make_xref: Callable[[str], docutils_nodes.Node]):
        self.root_node = addnodes.desc_inline("bzl", classes=["type-expr"])
        self.make_xref = make_xref
        self._doc_node_stack = [self.root_node]

    @classmethod
    def xrefs_from_type_expr(
        cls,
        type_expr_str: str,
        make_xref: Callable[[str], docutils_nodes.Node],
    ) -> docutils_nodes.Node:
        module = ast.parse(type_expr_str)
        visitor = cls(make_xref)
        visitor.visit(module.body[0])
        return visitor.root_node

    def _append(self, node: docutils_nodes.Node):
        self._doc_node_stack[-1] += node

    def _append_and_push(self, node: docutils_nodes.Node):
        self._append(node)
        self._doc_node_stack.append(node)

    def visit_Attribute(self, node: ast.Attribute):
        current = node
        parts = []
        while current:
            if isinstance(current, ast.Attribute):
                parts.append(current.attr)
                current = current.value
            elif isinstance(current, ast.Name):
                parts.append(current.id)
                break
            else:
                raise InvalidValueError(f"Unexpected Attribute.value node: {current}")
        dotted_name = ".".join(reversed(parts))
        self._append(self.make_xref(dotted_name))

    def visit_Constant(self, node: ast.Constant):
        if node.value is None:
            self._append(self.make_xref("None"))
        elif isinstance(node.value, str):
            self._append(self.make_xref(node.value))
        else:
            raise InvalidValueError(
                f"Unexpected Constant node value: ({type(node.value)}) {node.value=}"
            )

    def visit_Name(self, node: ast.Name):
        xref_node = self.make_xref(node.id)
        self._append(xref_node)

    def visit_BinOp(self, node: ast.BinOp):
        self.visit(node.left)
        self._append(addnodes.desc_sig_space())
        if isinstance(node.op, ast.BitOr):
            self._append(addnodes.desc_sig_punctuation("", "|"))
        else:
            raise InvalidValueError(f"Unexpected BinOp: {node}")
        self._append(addnodes.desc_sig_space())
        self.visit(node.right)

    def visit_Expr(self, node: ast.Expr):
        self.visit(node.value)

    def visit_Subscript(self, node: ast.Subscript):
        self.visit(node.value)
        self._append_and_push(addnodes.desc_type_parameter_list())
        self.visit(node.slice)
        self._doc_node_stack.pop()

    def visit_Tuple(self, node: ast.Tuple):
        for element in node.elts:
            self._append_and_push(addnodes.desc_type_parameter())
            self.visit(element)
            self._doc_node_stack.pop()

    def visit_List(self, node: ast.List):
        self._append_and_push(addnodes.desc_type_parameter_list())
        for element in node.elts:
            self._append_and_push(addnodes.desc_type_parameter())
            self.visit(element)
            self._doc_node_stack.pop()

    @override
    def generic_visit(self, node):
        raise InvalidValueError(f"Unexpected ast node: {type(node)} {node}")


class _BzlXrefField(docfields.Field):
    """Abstract base class to create cross references for fields."""

    @override
    def make_xrefs(
        self,
        rolename: str,
        domain: str,
        target: str,
        innernode: type[sphinx_typing.TextlikeNode] = addnodes.literal_emphasis,
        contnode: typing.Union[docutils_nodes.Node, None] = None,
        env: typing.Union[environment.BuildEnvironment, None] = None,
        inliner: typing.Union[states.Inliner, None] = None,
        location: typing.Union[docutils_nodes.Element, None] = None,
    ) -> list[docutils_nodes.Node]:
        if rolename in ("arg", "attr"):
            return self._make_xrefs_for_arg_attr(
                rolename, domain, target, innernode, contnode, env, inliner, location
            )
        else:
            return super().make_xrefs(
                rolename, domain, target, innernode, contnode, env, inliner, location
            )

    def _make_xrefs_for_arg_attr(
        self,
        rolename: str,
        domain: str,
        arg_name: str,
        innernode: type[sphinx_typing.TextlikeNode] = addnodes.literal_emphasis,
        contnode: typing.Union[docutils_nodes.Node, None] = None,
        env: typing.Union[environment.BuildEnvironment, None] = None,
        inliner: typing.Union[states.Inliner, None] = None,
        location: typing.Union[docutils_nodes.Element, None] = None,
    ) -> list[docutils_nodes.Node]:
        bzl_file = env.ref_context["bzl:file"]
        anchor_prefix = ".".join(env.ref_context["bzl:doc_id_stack"])
        if not anchor_prefix:
            raise InvalidValueError(
                f"doc_id_stack empty when processing arg {arg_name}"
            )
        index_description = f"{arg_name} ({self.name} in {bzl_file}%{anchor_prefix})"
        anchor_id = f"{anchor_prefix}.{arg_name}"
        full_id = _full_id_from_env(env, [arg_name])

        env.get_domain(domain).add_object(
            _ObjectEntry(
                full_id=full_id,
                display_name=arg_name,
                object_type=self.name,
                search_priority=1,
                index_entry=domains.IndexEntry(
                    name=arg_name,
                    subtype=_INDEX_SUBTYPE_NORMAL,
                    docname=env.docname,
                    anchor=anchor_id,
                    extra="",
                    qualifier="",
                    descr=index_description,
                ),
            ),
            # This allows referencing an arg as e.g `funcname.argname`
            alt_names=[anchor_id],
        )

        # Two changes to how arg xrefs are created:
        # 2. Use the full id instead of base name. This makes it unambiguous
        #    as to what it's referencing.
        pending_xref = super().make_xref(
            # The full_id is used as the target so its unambiguious.
            rolename,
            domain,
            f"{arg_name} <{full_id}>",
            innernode,
            contnode,
            env,
            inliner,
            location,
        )

        wrapper = docutils_nodes.inline(ids=[anchor_id])

        index_node = addnodes.index(
            entries=[
                _index_node_tuple(
                    "single", f"{self.name}; {index_description}", anchor_id
                ),
                _index_node_tuple("single", index_description, anchor_id),
            ]
        )
        wrapper += index_node
        wrapper += pending_xref
        return [wrapper]


class _BzlDocField(_BzlXrefField, docfields.Field):
    """A non-repeated field with xref support."""


class _BzlGroupedField(_BzlXrefField, docfields.GroupedField):
    """A repeated fieled grouped as a list with xref support."""


class _BzlCsvField(_BzlXrefField):
    """Field with a CSV list of values."""

    def __init__(self, *args, body_domain: str = "", **kwargs):
        super().__init__(*args, **kwargs)
        self._body_domain = body_domain

    def make_field(
        self,
        types: dict[str, list[docutils_nodes.Node]],
        domain: str,
        item: tuple,
        env: environment.BuildEnvironment = None,
        inliner: typing.Union[states.Inliner, None] = None,
        location: typing.Union[docutils_nodes.Element, None] = None,
    ) -> docutils_nodes.field:
        field_text = item[1][0].astext()
        parts = [p.strip() for p in field_text.split(",")]
        field_body = docutils_nodes.field_body()
        for _, is_last, part in _position_iter(parts):
            node = self.make_xref(
                self.bodyrolename,
                self._body_domain or domain,
                part,
                env=env,
                inliner=inliner,
                location=location,
            )
            field_body += node
            if not is_last:
                field_body += docutils_nodes.Text(", ")

        field_name = docutils_nodes.field_name("", self.label)
        return docutils_nodes.field("", field_name, field_body)


class _BzlCurrentFile(sphinx_docutils.SphinxDirective):
    """Sets what bzl file following directives are defined in.

    The directive's argument is an absolute Bazel label, e.g. `//foo:bar.bzl`
    or `@repo//foo:bar.bzl`. The repository portion is optional; if specified,
    it will override the `bzl_default_repository_name` configuration setting.

    Example MyST usage

    ```
    :::{bzl:currentfile} //my:file.bzl
    :::
    ```
    """

    has_content = False
    required_arguments = 1
    final_argument_whitespace = False

    @override
    def run(self) -> list[docutils_nodes.Node]:
        label = self.arguments[0].strip()
        repo, slashes, file_label = label.partition("//")
        file_label = slashes + file_label
        if not repo:
            repo = self.env.config.bzl_default_repository_name
        self.env.ref_context["bzl:repo"] = repo
        self.env.ref_context["bzl:file"] = file_label
        self.env.ref_context["bzl:object_id_stack"] = []
        self.env.ref_context["bzl:doc_id_stack"] = []
        return []


class _BzlAttrInfo(sphinx_docutils.SphinxDirective):
    has_content = False
    required_arguments = 1
    optional_arguments = 0
    option_spec = {
        "executable": docutils_directives.flag,
        "mandatory": docutils_directives.flag,
    }

    def run(self):
        content_node = docutils_nodes.paragraph("", "")
        content_node += docutils_nodes.paragraph(
            "", "mandatory" if "mandatory" in self.options else "optional"
        )
        if "executable" in self.options:
            content_node += docutils_nodes.paragraph("", "Must be an executable")

        return [content_node]


class _BzlObject(sphinx_directives.ObjectDescription[_BzlObjectId]):
    """Base class for describing a Bazel/Starlark object.

    This directive takes a single argument: a string name with optional
    function signature.

    * The name can be a dotted name, e.g. `a.b.foo`
    * The signature is in Python signature syntax, e.g. `foo(a=x) -> R`
    * The signature supports default values.
    * Arg type annotations are not supported; use `{bzl:type}` instead as
      part of arg/attr documentation.

    Example signatures:
      * `foo`
      * `foo(arg1, arg2)`
      * `foo(arg1, arg2=default) -> returntype`
    """

    option_spec = sphinx_directives.ObjectDescription.option_spec | {
        "origin-key": docutils_directives.unchanged,
    }

    @override
    def before_content(self) -> None:
        symbol_name = self.names[-1].symbol
        if symbol_name:
            self.env.ref_context["bzl:object_id_stack"].append(symbol_name)
            self.env.ref_context["bzl:doc_id_stack"].append(symbol_name)

    @override
    def transform_content(self, content_node: addnodes.desc_content) -> None:
        def first_child_with_class_name(
            root, class_name
        ) -> typing.Union[None, docutils_nodes.Element]:
            matches = root.findall(
                lambda node: isinstance(node, docutils_nodes.Element)
                and class_name in node["classes"]
            )
            found = next(matches, None)
            return found

        def match_arg_field_name(node):
            # fmt: off
            return (
                isinstance(node, docutils_nodes.field_name)
                and node.astext().startswith(("arg ", "attr "))
            )
            # fmt: on

        # Move the spans for the arg type and default value to be first.
        arg_name_fields = list(content_node.findall(match_arg_field_name))
        for arg_name_field in arg_name_fields:
            arg_body_field = arg_name_field.next_node(descend=False, siblings=True)
            # arg_type_node = first_child_with_class_name(arg_body_field, "arg-type-span")
            arg_type_node = first_child_with_class_name(arg_body_field, "type-expr")
            arg_default_node = first_child_with_class_name(
                arg_body_field, "default-value-span"
            )

            # Inserting into the body field itself causes the elements
            # to be grouped into the paragraph node containing the arg
            # name (as opposed to the paragraph node containing the
            # doc text)

            if arg_default_node:
                arg_default_node.parent.remove(arg_default_node)
                arg_body_field.insert(0, arg_default_node)

            if arg_type_node:
                arg_type_node.parent.remove(arg_type_node)
                decorated_arg_type_node = docutils_nodes.inline(
                    "",
                    "",
                    docutils_nodes.Text("("),
                    arg_type_node,
                    docutils_nodes.Text(") "),
                    classes=["arg-type-span"],
                )
                # arg_body_field.insert(0, arg_type_node)
                arg_body_field.insert(0, decorated_arg_type_node)

    @override
    def after_content(self) -> None:
        if self.names[-1].symbol:
            self.env.ref_context["bzl:object_id_stack"].pop()
            self.env.ref_context["bzl:doc_id_stack"].pop()

    # docs on how to build signatures:
    # https://www.sphinx-doc.org/en/master/extdev/nodes.html#sphinx.addnodes.desc_signature
    @override
    def handle_signature(
        self, sig_text: str, sig_node: addnodes.desc_signature
    ) -> _BzlObjectId:
        self._signature_add_object_type(sig_node)

        relative_name, lparen, params_text = sig_text.partition("(")
        if lparen:
            params_text = lparen + params_text

        relative_name = relative_name.strip()

        name_prefix, _, base_symbol_name = relative_name.rpartition(".")

        if name_prefix:
            # Respect whatever the signature wanted
            display_prefix = name_prefix
        else:
            # Otherwise, show the outermost name. This makes ctrl+f finding
            # for a symbol a bit easier.
            display_prefix = ".".join(self.env.ref_context["bzl:doc_id_stack"])
            _, _, display_prefix = display_prefix.rpartition(".")

        if display_prefix:
            display_prefix = display_prefix + "."
            sig_node += addnodes.desc_addname(display_prefix, display_prefix)
        sig_node += addnodes.desc_name(base_symbol_name, base_symbol_name)

        if type_expr := self.options.get("type"):

            def make_xref(name, title=None):
                content_node = addnodes.desc_type(name, name)
                return addnodes.pending_xref(
                    "",
                    content_node,
                    refdomain="bzl",
                    reftype="type",
                    reftarget=name,
                )

            attr_annotation_node = addnodes.desc_annotation(
                type_expr,
                "",
                addnodes.desc_sig_punctuation("", ":"),
                addnodes.desc_sig_space(),
                _TypeExprParser.xrefs_from_type_expr(type_expr, make_xref),
            )
            sig_node += attr_annotation_node

        if params_text:
            try:
                signature = inspect.signature_from_str(params_text)
            except SyntaxError:
                # Stardoc doesn't provide accurate info, so the reconstructed
                # signature might not be valid syntax. Rather than fail, just
                # provide a plain-text description of the approximate signature.
                # See https://github.com/bazelbuild/stardoc/issues/225
                sig_node += addnodes.desc_parameterlist(
                    # Offset by 1 to remove the surrounding parentheses
                    params_text[1:-1],
                    params_text[1:-1],
                )
            else:
                last_kind = None
                paramlist_node = addnodes.desc_parameterlist()
                for param in signature.parameters.values():
                    if param.kind == param.KEYWORD_ONLY and last_kind in (
                        param.POSITIONAL_OR_KEYWORD,
                        param.POSITIONAL_ONLY,
                        None,
                    ):
                        # Add separator for keyword only parameter: *
                        paramlist_node += addnodes.desc_parameter(
                            "", "", addnodes.desc_sig_operator("", "*")
                        )

                    last_kind = param.kind
                    node = addnodes.desc_parameter()
                    if param.kind == param.VAR_POSITIONAL:
                        node += addnodes.desc_sig_operator("", "*")
                    elif param.kind == param.VAR_KEYWORD:
                        node += addnodes.desc_sig_operator("", "**")

                    node += addnodes.desc_sig_name(rawsource="", text=param.name)
                    if param.default is not param.empty:
                        node += addnodes.desc_sig_operator("", "=")
                        node += docutils_nodes.inline(
                            "",
                            param.default,
                            classes=["default_value"],
                            support_smartquotes=False,
                        )
                    paramlist_node += node
                sig_node += paramlist_node

                if signature.return_annotation is not signature.empty:
                    sig_node += addnodes.desc_returns("", signature.return_annotation)

        obj_id = _BzlObjectId.from_env(self.env, symbol=relative_name)

        sig_node["bzl:object_id"] = obj_id.full_id
        return obj_id

    def _signature_add_object_type(self, sig_node: addnodes.desc_signature):
        if sig_object_type := self._get_signature_object_type():
            sig_node += addnodes.desc_annotation("", self._get_signature_object_type())
            sig_node += addnodes.desc_sig_space()

    @override
    def add_target_and_index(
        self, obj_desc: _BzlObjectId, sig: str, sig_node: addnodes.desc_signature
    ) -> None:
        super().add_target_and_index(obj_desc, sig, sig_node)
        if obj_desc.symbol:
            display_name = obj_desc.symbol
            location = obj_desc.label
            if obj_desc.namespace:
                location += f"%{obj_desc.namespace}"
        else:
            display_name = obj_desc.target_name
            location = obj_desc.package

        anchor_prefix = ".".join(self.env.ref_context["bzl:doc_id_stack"])
        if anchor_prefix:
            anchor_id = f"{anchor_prefix}.{obj_desc.doc_id}"
        else:
            anchor_id = obj_desc.doc_id

        sig_node["ids"].append(anchor_id)

        object_type_display = self._get_object_type_display_name()
        index_description = f"{display_name} ({object_type_display} in {location})"
        self.indexnode["entries"].extend(
            _index_node_tuple("single", f"{index_type}; {index_description}", anchor_id)
            for index_type in [object_type_display] + self._get_additional_index_types()
        )
        self.indexnode["entries"].append(
            _index_node_tuple("single", index_description, anchor_id),
        )

        object_entry = _ObjectEntry(
            full_id=obj_desc.full_id,
            display_name=display_name,
            object_type=self.objtype,
            search_priority=1,
            index_entry=domains.IndexEntry(
                name=display_name,
                subtype=_INDEX_SUBTYPE_NORMAL,
                docname=self.env.docname,
                anchor=anchor_id,
                extra="",
                qualifier="",
                descr=index_description,
            ),
        )

        alt_names = []
        if origin_key := self.options.get("origin-key"):
            alt_names.append(
                origin_key
                # Options require \@ for leading @, but don't
                # remove the escaping slash, so we have to do it manually
                .lstrip("\\")
            )
        extra_alt_names = self._get_alt_names(object_entry)
        alt_names.extend(extra_alt_names)

        self.env.get_domain(self.domain).add_object(object_entry, alt_names=alt_names)

    def _get_additional_index_types(self):
        return []

    @override
    def _object_hierarchy_parts(
        self, sig_node: addnodes.desc_signature
    ) -> tuple[str, ...]:
        return _parse_full_id(sig_node["bzl:object_id"])

    @override
    def _toc_entry_name(self, sig_node: addnodes.desc_signature) -> str:
        return sig_node["_toc_parts"][-1]

    def _get_object_type_display_name(self) -> str:
        return self.env.get_domain(self.domain).object_types[self.objtype].lname

    def _get_signature_object_type(self) -> str:
        return self._get_object_type_display_name()

    def _get_alt_names(self, object_entry):
        alt_names = []
        full_id = object_entry.full_id
        label, _, symbol = full_id.partition("%")
        if symbol:
            # Allow referring to the file-relative fully qualified symbol name
            alt_names.append(symbol)
            if "." in symbol:
                # Allow referring to the last component of the symbol
                alt_names.append(symbol.split(".")[-1])
        else:
            # Otherwise, it's a target. Allow referring to just the target name
            _, _, target_name = label.partition(":")
            alt_names.append(target_name)

        return alt_names


class _BzlCallable(_BzlObject):
    """Abstract base class for objects that are callable."""


class _BzlTypedef(_BzlObject):
    """Documents a typedef.

    A typedef describes objects with well known attributes.

    `````
    ::::{bzl:typedef} Square

    :::{bzl:field} width
    :type: int
    :::

    :::{bzl:function} new(size)
    :::

    :::{bzl:function} area()
    :::
    ::::
    `````
    """


class _BzlProvider(_BzlObject):
    """Documents a provider type.

    Example MyST usage

    ```
    ::::{bzl:provider} MyInfo

    Docs about MyInfo

    :::{bzl:provider-field} some_field
    :type: depset[str]
    :::
    ::::
    ```
    """


class _BzlField(_BzlObject):
    """Documents a field of a provider.

    Fields can optionally have a type specified using the `:type:` option.

    The type can be any type expression understood by the `{bzl:type}` role.

    ```
    :::{bzl:provider-field} foo
    :type: str
    :::
    ```
    """

    option_spec = _BzlObject.option_spec.copy()
    option_spec.update(
        {
            "type": docutils_directives.unchanged,
        }
    )

    @override
    def _get_signature_object_type(self) -> str:
        return ""

    @override
    def _get_alt_names(self, object_entry):
        alt_names = super()._get_alt_names(object_entry)
        _, _, symbol = object_entry.full_id.partition("%")
        # Allow refering to `mod_ext_name.tag_name`, even if the extension
        # is nested within another object
        alt_names.append(".".join(symbol.split(".")[-2:]))
        return alt_names


class _BzlProviderField(_BzlField):
    pass


class _BzlRepositoryRule(_BzlCallable):
    """Documents a repository rule.

    Doc fields:
    * attr: Documents attributes of the rule. Takes a single arg, the
      attribute name. Can be repeated. The special roles `{default-value}`
      and `{arg-type}` can be used to indicate the default value and
      type of attribute, respectively.
    * environment-variables: a CSV list of environment variable names.
      They will be cross referenced with matching environment variables.

    Example MyST usage

    ```
    :::{bzl:repo-rule} myrule(foo)

    :attr foo: {default-value}`"foo"` {arg-type}`attr.string` foo doc string

    :environment-variables: FOO, BAR
    :::
    ```
    """

    doc_field_types = [
        _BzlGroupedField(
            "attr",
            label=_("Attributes"),
            names=["attr"],
            rolename="attr",
            can_collapse=False,
        ),
        _BzlCsvField(
            "environment-variables",
            label=_("Environment Variables"),
            names=["environment-variables"],
            body_domain="std",
            bodyrolename="envvar",
            has_arg=False,
        ),
    ]

    @override
    def _get_signature_object_type(self) -> str:
        return "repo rule"


class _BzlRule(_BzlCallable):
    """Documents a rule.

    Doc fields:
    * attr: Documents attributes of the rule. Takes a single arg, the
      attribute name. Can be repeated. The special roles `{default-value}`
      and `{arg-type}` can be used to indicate the default value and
      type of attribute, respectively.
    * provides: A type expression of the provider types the rule provides.
      To indicate different groupings, use `|` and `[]`. For example,
      `FooInfo | [BarInfo, BazInfo]` means it provides either `FooInfo`
      or both of `BarInfo` and `BazInfo`.

    Example MyST usage

    ```
    :::{bzl:repo-rule} myrule(foo)

    :attr foo: {default-value}`"foo"` {arg-type}`attr.string` foo doc string

    :provides: FooInfo | BarInfo
    :::
    ```
    """

    doc_field_types = [
        _BzlGroupedField(
            "attr",
            label=_("Attributes"),
            names=["attr"],
            rolename="attr",
            can_collapse=False,
        ),
        _BzlDocField(
            "provides",
            label="Provides",
            has_arg=False,
            names=["provides"],
            bodyrolename="type",
        ),
    ]


class _BzlAspect(_BzlObject):
    """Documents an aspect.

    Doc fields:
    * attr: Documents attributes of the aspect. Takes a single arg, the
      attribute name. Can be repeated. The special roles `{default-value}`
      and `{arg-type}` can be used to indicate the default value and
      type of attribute, respectively.
    * aspect-attributes: A CSV list of attribute names the aspect
      propagates along.

    Example MyST usage

    ```
    :::{bzl:repo-rule} myaspect

    :attr foo: {default-value}`"foo"` {arg-type}`attr.string` foo doc string

    :aspect-attributes: srcs, deps
    :::
    ```
    """

    doc_field_types = [
        _BzlGroupedField(
            "attr",
            label=_("Attributes"),
            names=["attr"],
            rolename="attr",
            can_collapse=False,
        ),
        _BzlCsvField(
            "aspect-attributes",
            label=_("Aspect Attributes"),
            names=["aspect-attributes"],
            has_arg=False,
        ),
    ]


class _BzlFunction(_BzlCallable):
    """Documents a general purpose function.

    Doc fields:
    * arg: Documents the arguments of the function. Takes a single arg, the
      arg name. Can be repeated. The special roles `{default-value}`
      and `{arg-type}` can be used to indicate the default value and
      type of attribute, respectively.
    * returns: Documents what the function returns. The special role
      `{return-type}` can be used to indicate the return type of the function.

    Example MyST usage

    ```
    :::{bzl:function} myfunc(a, b=None) -> bool

    :arg a: {arg-type}`str` some arg doc
    :arg b: {arg-type}`int | None` {default-value}`42` more arg doc
    :returns: {return-type}`bool` doc about return value.
    :::
    ```
    """

    doc_field_types = [
        _BzlGroupedField(
            "arg",
            label=_("Args"),
            names=["arg"],
            rolename="arg",
            can_collapse=False,
        ),
        docfields.Field(
            "returns",
            label=_("Returns"),
            has_arg=False,
            names=["returns"],
        ),
    ]

    @override
    def _get_signature_object_type(self) -> str:
        return ""


class _BzlModuleExtension(_BzlObject):
    """Documents a module_extension.

    Doc fields:
    * os-dependent: Documents if the module extension depends on the host
      architecture.
    * arch-dependent: Documents if the module extension depends on the host
      architecture.
    * environment-variables: a CSV list of environment variable names.
      They will be cross referenced with matching environment variables.

    Tag classes are documented using the bzl:tag-class directives within
    this directive.

    Example MyST usage:

    ```
    ::::{bzl:module-extension} myext

    :os-dependent: True
    :arch-dependent: False

    :::{bzl:tag-class} mytag(myattr)

    :attr myattr:
      {arg-type}`attr.string_list`
      doc for attribute
    :::
    ::::
    ```
    """

    doc_field_types = [
        _BzlDocField(
            "os-dependent",
            label="OS Dependent",
            has_arg=False,
            names=["os-dependent"],
        ),
        _BzlDocField(
            "arch-dependent",
            label="Arch Dependent",
            has_arg=False,
            names=["arch-dependent"],
        ),
        _BzlCsvField(
            "environment-variables",
            label=_("Environment Variables"),
            names=["environment-variables"],
            body_domain="std",
            bodyrolename="envvar",
            has_arg=False,
        ),
    ]

    @override
    def _get_signature_object_type(self) -> str:
        return "module ext"


class _BzlTagClass(_BzlCallable):
    """Documents a tag class for a module extension.

    Doc fields:
    * attr: Documents attributes of the tag class. Takes a single arg, the
      attribute name. Can be repeated. The special roles `{default-value}`
      and `{arg-type}` can be used to indicate the default value and
      type of attribute, respectively.

    Example MyST usage, note that this directive should be nested with
    a `bzl:module-extension` directive.

    ```
    :::{bzl:tag-class} mytag(myattr)

    :attr myattr:
      {arg-type}`attr.string_list`
      doc for attribute
    :::
    ```
    """

    doc_field_types = [
        _BzlGroupedField(
            "arg",
            label=_("Attributes"),
            names=["attr"],
            rolename="arg",
            can_collapse=False,
        ),
    ]

    @override
    def _get_signature_object_type(self) -> str:
        return ""

    @override
    def _get_alt_names(self, object_entry):
        alt_names = super()._get_alt_names(object_entry)
        _, _, symbol = object_entry.full_id.partition("%")
        # Allow refering to `ProviderName.field`, even if the provider
        # is nested within another object
        alt_names.append(".".join(symbol.split(".")[-2:]))
        return alt_names


class _TargetType(enum.Enum):
    TARGET = "target"
    FLAG = "flag"


class _BzlTarget(_BzlObject):
    """Documents an arbitrary target."""

    _TARGET_TYPE = _TargetType.TARGET

    def handle_signature(self, sig_text, sig_node):
        self._signature_add_object_type(sig_node)
        if ":" in sig_text:
            package, target_name = sig_text.split(":", 1)
        else:
            target_name = sig_text
            package = self.env.ref_context["bzl:file"]
            package = package[: package.find(":BUILD")]

        package = package + ":"
        if self._TARGET_TYPE == _TargetType.FLAG:
            sig_node += addnodes.desc_addname("--", "--")
        sig_node += addnodes.desc_addname(package, package)
        sig_node += addnodes.desc_name(target_name, target_name)

        obj_id = _BzlObjectId.from_env(self.env, label=package + target_name)
        sig_node["bzl:object_id"] = obj_id.full_id
        return obj_id

    @override
    def _get_signature_object_type(self) -> str:
        # We purposely return empty here because having "target" in front
        # of every label isn't very helpful
        return ""


# TODO: Integrate with the option directive, since flags are options, afterall.
# https://www.sphinx-doc.org/en/master/usage/domains/standard.html#directive-option
class _BzlFlag(_BzlTarget):
    """Documents a flag"""

    _TARGET_TYPE = _TargetType.FLAG

    @override
    def _get_signature_object_type(self) -> str:
        return "flag"

    def _get_additional_index_types(self):
        return ["target"]


class _DefaultValueRole(sphinx_docutils.SphinxRole):
    """Documents the default value for an arg or attribute.

    This is a special role used within `:arg:` and `:attr:` doc fields to
    indicate the default value. The rendering process looks for this role
    and reformats and moves its content for better display.

    Styling can be customized by matching the `.default_value` class.
    """

    def run(self) -> _RoleRunResult:
        node = docutils_nodes.emphasis(
            "",
            "(default ",
            docutils_nodes.inline("", self.text, classes=["sig", "default_value"]),
            docutils_nodes.Text(") "),
            classes=["default-value-span"],
        )
        return ([node], [])


class _TypeRole(sphinx_docutils.SphinxRole):
    """Documents a type (or type expression) with crossreferencing.

    This is an inline role used to create cross references to other types.

    The content is interpreted as a reference to a type or an expression
    of types. The syntax uses Python-style sytax with `|` and `[]`, e.g.
    `foo.MyType | str | list[str] | dict[str, int]`. Each symbolic name
    will be turned into a cross reference; see the domain's documentation
    for how to reference objects.

    Example MyST usage:

    ```
    This function accepts {bzl:type}`str | list[str]` for usernames
    ```
    """

    def __init__(self):
        super().__init__()
        self._xref = roles.XRefRole()

    def run(self) -> _RoleRunResult:
        outer_messages = []

        def make_xref(name):
            nodes, msgs = self._xref(
                "bzl:type",
                name,
                name,
                self.lineno,
                self.inliner,
                self.options,
                self.content,
            )
            outer_messages.extend(msgs)
            if len(nodes) == 1:
                return nodes[0]
            else:
                return docutils_nodes.inline("", "", nodes)

        root = _TypeExprParser.xrefs_from_type_expr(self.text, make_xref)
        return ([root], outer_messages)


class _ReturnTypeRole(_TypeRole):
    """Documents the return type for function.

    This is a special role used within `:returns:` doc fields to
    indicate the return type of the function. The rendering process looks for
    this role and reformats and moves its content for better display.

    Example MyST Usage

    ```
    :::{bzl:function} foo()

    :returns: {return-type}`list[str]`
    :::
    ```
    """

    def run(self) -> _RoleRunResult:
        nodes, messages = super().run()
        nodes.append(docutils_nodes.Text(" -- "))
        return nodes, messages


class _RequiredProvidersRole(_TypeRole):
    """Documents the providers an attribute requires.

    This is a special role used within `:arg:` or `:attr:` doc fields to
    indicate the types of providers that are required. The rendering process
    looks for this role and reformats its content for better display, but its
    position is left as-is; typically it would be its own paragraph near the
    end of the doc.

    The syntax is a pipe (`|`) delimited list of types or groups of types,
    where groups are indicated using `[...]`. e.g, to express that FooInfo OR
    (both of BarInfo and BazInfo) are supported, write `FooInfo | [BarInfo,
    BazInfo]`

    Example MyST Usage

    ```
    :::{bzl:rule} foo(bar)

    :attr bar: My attribute doc

      {required-providers}`CcInfo | [PyInfo, JavaInfo]`
    :::
    ```
    """

    def run(self) -> _RoleRunResult:
        xref_nodes, messages = super().run()
        nodes = [
            docutils_nodes.emphasis("", "Required providers: "),
        ] + xref_nodes
        return nodes, messages


class _BzlIndex(domains.Index):
    """An index of a bzl file's objects.

    NOTE: This generates the entries for the *domain specific* index
    (bzl-index.html), not the general index (genindex.html). To affect
    the general index, index nodes and directives must be used (grep
    for `self.indexnode`).
    """

    name = "index"
    localname = "Bazel/Starlark Object Index"
    shortname = "Bzl"

    def generate(
        self, docnames: Iterable[str] = None
    ) -> tuple[list[tuple[str, list[domains.IndexEntry]]], bool]:
        content = collections.defaultdict(list)

        # sort the list of objects in alphabetical order
        objects = self.domain.data["objects"].values()
        objects = sorted(objects, key=lambda obj: obj.index_entry.name)

        # Group by first letter
        for entry in objects:
            index_entry = entry.index_entry
            content[index_entry.name[0].lower()].append(index_entry)

        # convert the dict to the sorted list of tuples expected
        content = sorted(content.items())

        return content, True


class _BzlDomain(domains.Domain):
    """Domain for Bazel/Starlark objects.

    Directives

    There are directives for defining Bazel objects and their functionality.
    See the respective directive classes for details.

    Public Crossreferencing Roles

    These are roles that can be used in docs to create cross references.

    Objects are fully identified using dotted notation converted from the Bazel
    label and symbol name within a `.bzl` file. The `@`, `/` and `:` characters
    are converted to dots (with runs removed), and `.bzl` is removed from file
    names. The dotted path of a symbol in the bzl file is appended. For example,
    the `paths.join` function in `@bazel_skylib//lib:paths.bzl` would be
    identified as `bazel_skylib.lib.paths.paths.join`.

    Shorter identifiers can be used. Within a project, the repo name portion
    can be omitted. Within a file, file-relative names can be used.

    * obj: Used to reference a single object without concern for its type.
      This roles searches all object types for a name that matches the given
      value. Example usage in MyST:
      ```
      {bzl:obj}`repo.pkg.file.my_function`
      ```

    * type: Transforms a type expression into cross references for objects
      with object type "type". For example, it parses `int | list[str]` into
      three links for each component part.

    Public Typography Roles

    These are roles used for special purposes to aid documentation.

    * default-value: The default value for an argument or attribute. Only valid
      to use within arg or attribute documentation. See `_DefaultValueRole` for
      details.
    * required-providers: The providers an attribute requires. Only
      valud to use within an attribute documentation. See
      `_RequiredProvidersRole` for details.
    * return-type: The type of value a function returns. Only valid
      within a function's return doc field. See `_ReturnTypeRole` for details.

    Object Types

    These are the types of objects that this domain keeps in its index.

    * arg: An argument to a function or macro.
    * aspect: A Bazel `aspect`.
    * attribute: An input to a rule (regular, repository, aspect, or module
      extension).
    * method: A function bound to an instance of a struct acting as a type.
    * module-extension: A Bazel `module_extension`.
    * provider: A Bazel `provider`.
    * provider-field: A field of a provider.
    * repo-rule: A Bazel `repository_rule`.
    * rule: A regular Bazel `rule`.
    * tag-class: A Bazel `tag_class` of a `module_extension`.
    * target: A Bazel target.
    * type: A builtin Bazel type or user-defined structural type. User defined
      structual types are typically instances `struct` created using a function
      that acts as a constructor with implicit state bound using closures.
    """

    name = "bzl"
    label = "Bzl"

    # NOTE: Most every object type has "obj" as one of the roles because
    # an object type's role determine what reftypes (cross referencing) can
    # refer to it. By having "obj" for all of them, it allows writing
    # :bzl:obj`foo` to restrict object searching to the bzl domain. Under the
    # hood, this domain translates requests for the :any: role as lookups for
    # :obj:.
    # NOTE: We also use these object types for categorizing things in the
    # generated index page.
    object_types = {
        "arg": domains.ObjType("arg", "arg", "obj"),  # macro/function arg
        "aspect": domains.ObjType("aspect", "aspect", "obj"),
        "attr": domains.ObjType("attr", "attr", "obj"),  # rule attribute
        "function": domains.ObjType("function", "func", "obj"),
        "method": domains.ObjType("method", "method", "obj"),
        "module-extension": domains.ObjType(
            "module extension", "module_extension", "obj"
        ),
        # Providers are close enough to types that we include "type". This
        # also makes :type: Foo work in directive options.
        "provider": domains.ObjType("provider", "provider", "type", "obj"),
        "provider-field": domains.ObjType("provider field", "provider-field", "obj"),
        "field": domains.ObjType("field", "field", "obj"),
        "repo-rule": domains.ObjType("repository rule", "repo_rule", "obj"),
        "rule": domains.ObjType("rule", "rule", "obj"),
        "tag-class": domains.ObjType("tag class", "tag_class", "obj"),
        "target": domains.ObjType("target", "target", "obj"),  # target in a build file
        # Flags are also targets, so include "target" for xref'ing
        "flag": domains.ObjType("flag", "flag", "target", "obj"),
        # types are objects that have a constructor and methods/attrs
        "type": domains.ObjType("type", "type", "obj"),
        "typedef": domains.ObjType("typedef", "typedef", "type", "obj"),
    }

    # This controls:
    # * What is recognized when parsing, e.g. ":bzl:ref:`foo`" requires
    # "ref" to be in the role dict below.
    roles = {
        "arg": roles.XRefRole(),
        "attr": roles.XRefRole(),
        "default-value": _DefaultValueRole(),
        "flag": roles.XRefRole(),
        "obj": roles.XRefRole(),
        "required-providers": _RequiredProvidersRole(),
        "return-type": _ReturnTypeRole(),
        "rule": roles.XRefRole(),
        "target": roles.XRefRole(),
        "type": _TypeRole(),
    }
    # NOTE: Directives that have a corresponding object type should use
    # the same key for both directive and object type. Some directives
    # look up their corresponding object type.
    directives = {
        "aspect": _BzlAspect,
        "currentfile": _BzlCurrentFile,
        "function": _BzlFunction,
        "module-extension": _BzlModuleExtension,
        "provider": _BzlProvider,
        "typedef": _BzlTypedef,
        "provider-field": _BzlProviderField,
        "field": _BzlField,
        "repo-rule": _BzlRepositoryRule,
        "rule": _BzlRule,
        "tag-class": _BzlTagClass,
        "target": _BzlTarget,
        "flag": _BzlFlag,
        "attr-info": _BzlAttrInfo,
    }
    indices = {
        _BzlIndex,
    }

    # NOTE: When adding additional data keys, make sure to update
    # merge_domaindata
    initial_data = {
        # All objects; keyed by full id
        # dict[str, _ObjectEntry]
        "objects": {},
        #  dict[str, dict[str, _ObjectEntry]]
        "objects_by_type": {},
        # Objects within each doc
        # dict[str, dict[str, _ObjectEntry]]
        "doc_names": {},
        # Objects by a shorter or alternative name
        # dict[str, dict[str id, _ObjectEntry]]
        "alt_names": {},
    }

    @override
    def get_full_qualified_name(
        self, node: docutils_nodes.Element
    ) -> typing.Union[str, None]:
        bzl_file = node.get("bzl:file")
        symbol_name = node.get("bzl:symbol")
        ref_target = node.get("reftarget")
        return ".".join(filter(None, [bzl_file, symbol_name, ref_target]))

    @override
    def get_objects(self) -> Iterable[_GetObjectsTuple]:
        for entry in self.data["objects"].values():
            yield entry.to_get_objects_tuple()

    @override
    def resolve_any_xref(
        self,
        env: environment.BuildEnvironment,
        fromdocname: str,
        builder: builders.Builder,
        target: str,
        node: addnodes.pending_xref,
        contnode: docutils_nodes.Element,
    ) -> list[tuple[str, docutils_nodes.Element]]:
        del env, node  # Unused
        entry = self._find_entry_for_xref(fromdocname, "obj", target)
        if not entry:
            return []
        to_docname = entry.index_entry.docname
        to_anchor = entry.index_entry.anchor
        ref_node = sphinx_nodes.make_refnode(
            builder, fromdocname, to_docname, to_anchor, contnode, title=to_anchor
        )

        matches = [(f"bzl:{entry.object_type}", ref_node)]
        return matches

    @override
    def resolve_xref(
        self,
        env: environment.BuildEnvironment,
        fromdocname: str,
        builder: builders.Builder,
        typ: str,
        target: str,
        node: addnodes.pending_xref,
        contnode: docutils_nodes.Element,
    ) -> typing.Union[docutils_nodes.Element, None]:
        _log_debug(
            "resolve_xref: fromdocname=%s, typ=%s, target=%s", fromdocname, typ, target
        )
        del env, node  # Unused
        entry = self._find_entry_for_xref(fromdocname, typ, target)
        if not entry:
            return None

        to_docname = entry.index_entry.docname
        to_anchor = entry.index_entry.anchor
        return sphinx_nodes.make_refnode(
            builder, fromdocname, to_docname, to_anchor, contnode, title=to_anchor
        )

    def _find_entry_for_xref(
        self, fromdocname: str, object_type: str, target: str
    ) -> typing.Union[_ObjectEntry, None]:
        if target.startswith("--"):
            target = target.strip("-")
            object_type = "flag"

        # Allow using parentheses, e.g. `foo()` or `foo(x=...)`
        target, _, _ = target.partition("(")

        # Elide the value part of --foo=bar flags
        # Note that the flag value could contain `=`
        if "=" in target:
            target = target[: target.find("=")]

        if target in self.data["doc_names"].get(fromdocname, {}):
            entry = self.data["doc_names"][fromdocname][target]
            # Prevent a local doc name masking a global alt name when its of
            # a different type. e.g. when the macro `foo` refers to the
            # rule `foo` in another doc.
            if object_type in self.object_types[entry.object_type].roles:
                return entry

        if object_type == "obj":
            search_space = self.data["objects"]
        else:
            search_space = self.data["objects_by_type"].get(object_type, {})
        if target in search_space:
            return search_space[target]

        _log_debug("find_entry: alt_names=%s", sorted(self.data["alt_names"].keys()))
        if target in self.data["alt_names"]:
            # Give preference to shorter object ids. This is a work around
            # to allow e.g. `FooInfo` to refer to the FooInfo type rather than
            # the `FooInfo` constructor.
            entries = sorted(
                self.data["alt_names"][target].items(), key=lambda item: len(item[0])
            )
            for _, entry in entries:
                if object_type in self.object_types[entry.object_type].roles:
                    return entry

        return None

    def add_object(self, entry: _ObjectEntry, alt_names=None) -> None:
        _log_debug(
            "add_object: full_id=%s, object_type=%s, alt_names=%s",
            entry.full_id,
            entry.object_type,
            alt_names,
        )
        if entry.full_id in self.data["objects"]:
            existing = self.data["objects"][entry.full_id]
            raise Exception(
                f"Object {entry.full_id} already registered: "
                + f"existing={existing}, incoming={entry}"
            )
        self.data["objects"][entry.full_id] = entry
        self.data["objects_by_type"].setdefault(entry.object_type, {})
        self.data["objects_by_type"][entry.object_type][entry.full_id] = entry

        repo, label, symbol = _parse_full_id(entry.full_id)
        if symbol:
            base_name = symbol.split(".")[-1]
        else:
            base_name = label.split(":")[-1]

        if alt_names is not None:
            alt_names = list(alt_names)
        # Add the repo-less version as an alias
        alt_names.append(label + (f"%{symbol}" if symbol else ""))

        for alt_name in sorted(set(alt_names)):
            self.data["alt_names"].setdefault(alt_name, {})
            self.data["alt_names"][alt_name][entry.full_id] = entry

        docname = entry.index_entry.docname
        self.data["doc_names"].setdefault(docname, {})
        self.data["doc_names"][docname][base_name] = entry

    def merge_domaindata(
        self, docnames: list[str], otherdata: dict[str, typing.Any]
    ) -> None:
        # Merge in simple dict[key, value] data
        for top_key in ("objects",):
            self.data[top_key].update(otherdata.get(top_key, {}))

        # Merge in two-level dict[top_key, dict[sub_key, value]] data
        for top_key in ("objects_by_type", "doc_names", "alt_names"):
            existing_top_map = self.data[top_key]
            for sub_key, sub_values in otherdata.get(top_key, {}).items():
                if sub_key not in existing_top_map:
                    existing_top_map[sub_key] = sub_values
                else:
                    existing_top_map[sub_key].update(sub_values)


def _on_missing_reference(app, env: environment.BuildEnvironment, node, contnode):
    if node["refdomain"] != "bzl":
        return None
    if node["reftype"] != "type":
        return None

    # There's no Bazel docs for None, so prevent missing xrefs warning
    if node["reftarget"] == "None":
        return contnode
    return None


def setup(app):
    app.add_domain(_BzlDomain)

    app.add_config_value(
        "bzl_default_repository_name",
        default=os.environ.get("SPHINX_BZL_DEFAULT_REPOSITORY_NAME", "@_main"),
        rebuild="env",
        types=[str],
    )
    app.connect("missing-reference", _on_missing_reference)

    # Pygments says it supports starlark, but it doesn't seem to actually
    # recognize `starlark` as a name. So just manually map it to python.
    app.add_lexer("starlark", lexer_classes["python"])
    app.add_lexer("bzl", lexer_classes["python"])

    return {
        "version": "1.0.0",
        "parallel_read_safe": True,
        "parallel_write_safe": True,
    }
