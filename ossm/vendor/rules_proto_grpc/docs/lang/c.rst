:author: rules_proto_grpc
:description: rules_proto_grpc Bazel rules for C
:keywords: Bazel, Protobuf, gRPC, Protocol Buffers, Rules, Build, Starlark, C


C
=

Rules for generating C protobuf ``.c`` & ``.h`` files and libraries using `upb <https://github.com/protocolbuffers/upb>`_. Libraries are created with the Bazel native ``cc_library``

.. list-table:: Rules
   :widths: 1 2
   :header-rows: 1

   * - Rule
     - Description
   * - `c_proto_compile`_
     - Generates C protobuf ``.h`` & ``.c`` files
   * - `c_proto_library`_
     - Generates a C protobuf library using ``cc_library``, with dependencies linked

.. _c_proto_compile:

c_proto_compile
---------------

.. warning:: This rule is experimental. It may not work correctly or may change in future releases!

Generates C protobuf ``.h`` & ``.c`` files

Example
*******

Full example project can be found `here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/c/c_proto_compile>`__

``WORKSPACE``
^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//c:repositories.bzl", rules_proto_grpc_c_repos = "c_repos")
   
   rules_proto_grpc_c_repos()
   
   load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")
   
   grpc_deps()
   
   load("@com_github_grpc_grpc//bazel:grpc_extra_deps.bzl", "grpc_extra_deps")
   
   grpc_extra_deps()

``BUILD.bazel``
^^^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//c:defs.bzl", "c_proto_compile")
   
   c_proto_compile(
       name = "person_c_proto",
       protos = ["@rules_proto_grpc//example/proto:person_proto"],
   )
   
   c_proto_compile(
       name = "place_c_proto",
       protos = ["@rules_proto_grpc//example/proto:place_proto"],
   )
   
   c_proto_compile(
       name = "thing_c_proto",
       protos = ["@rules_proto_grpc//example/proto:thing_proto"],
   )

Attributes
**********

.. list-table:: Attributes for c_proto_compile
   :widths: 1 1 1 1 4
   :header-rows: 1

   * - Name
     - Type
     - Mandatory
     - Default
     - Description
   * - ``protos``
     - ``label_list``
     - true
     - 
     - List of labels that provide the ``ProtoInfo`` provider (such as ``proto_library`` from ``rules_proto``)
   * - ``options``
     - ``string_list_dict``
     - false
     - ``[]``
     - Extra options to pass to plugins, as a dict of plugin label -> list of strings. The key * can be used exclusively to apply to all plugins
   * - ``verbose``
     - ``int``
     - false
     - ``0``
     - The verbosity level. Supported values and results are 0: Show nothing, 1: Show command, 2: Show command and sandbox after running protoc, 3: Show command and sandbox before and after running protoc, 4. Show env, command, expected outputs and sandbox before and after running protoc
   * - ``prefix_path``
     - ``string``
     - false
     - ``""``
     - Path to prefix to the generated files in the output directory
   * - ``extra_protoc_args``
     - ``string_list``
     - false
     - ``[]``
     - A list of extra command line arguments to pass directly to protoc, not as plugin options
   * - ``extra_protoc_files``
     - ``label_list``
     - false
     - ``[]``
     - List of labels that provide extra files to be available during protoc execution
   * - ``output_mode``
     - ``string``
     - false
     - ``PREFIXED``
     - The output mode for the target. PREFIXED (the default) will output to a directory named by the target within the current package root, NO_PREFIX will output directly to the current package. Using NO_PREFIX may lead to conflicting writes

Plugins
*******

- `@rules_proto_grpc//c:upb_plugin <https://github.com/rules-proto-grpc/rules_proto_grpc/blob/master/c/BUILD.bazel>`__

.. _c_proto_library:

c_proto_library
---------------

.. warning:: This rule is experimental. It may not work correctly or may change in future releases!

Generates a C protobuf library using ``cc_library``, with dependencies linked

Example
*******

Full example project can be found `here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/c/c_proto_library>`__

``WORKSPACE``
^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//c:repositories.bzl", rules_proto_grpc_c_repos = "c_repos")
   
   rules_proto_grpc_c_repos()
   
   load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")
   
   grpc_deps()
   
   load("@com_github_grpc_grpc//bazel:grpc_extra_deps.bzl", "grpc_extra_deps")
   
   grpc_extra_deps()

``BUILD.bazel``
^^^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//c:defs.bzl", "c_proto_library")
   
   c_proto_library(
       name = "proto_c_proto",
       importpath = "github.com/rules-proto-grpc/rules_proto_grpc/example/proto",
       protos = [
           "@com_google_protobuf//:any_proto",
           "@rules_proto_grpc//example/proto:person_proto",
           "@rules_proto_grpc//example/proto:place_proto",
           "@rules_proto_grpc//example/proto:thing_proto",
       ],
   )

Attributes
**********

.. list-table:: Attributes for c_proto_library
   :widths: 1 1 1 1 4
   :header-rows: 1

   * - Name
     - Type
     - Mandatory
     - Default
     - Description
   * - ``protos``
     - ``label_list``
     - true
     - 
     - List of labels that provide the ``ProtoInfo`` provider (such as ``proto_library`` from ``rules_proto``)
   * - ``options``
     - ``string_list_dict``
     - false
     - ``[]``
     - Extra options to pass to plugins, as a dict of plugin label -> list of strings. The key * can be used exclusively to apply to all plugins
   * - ``verbose``
     - ``int``
     - false
     - ``0``
     - The verbosity level. Supported values and results are 0: Show nothing, 1: Show command, 2: Show command and sandbox after running protoc, 3: Show command and sandbox before and after running protoc, 4. Show env, command, expected outputs and sandbox before and after running protoc
   * - ``prefix_path``
     - ``string``
     - false
     - ``""``
     - Path to prefix to the generated files in the output directory
   * - ``extra_protoc_args``
     - ``string_list``
     - false
     - ``[]``
     - A list of extra command line arguments to pass directly to protoc, not as plugin options
   * - ``extra_protoc_files``
     - ``label_list``
     - false
     - ``[]``
     - List of labels that provide extra files to be available during protoc execution
   * - ``output_mode``
     - ``string``
     - false
     - ``PREFIXED``
     - The output mode for the target. PREFIXED (the default) will output to a directory named by the target within the current package root, NO_PREFIX will output directly to the current package. Using NO_PREFIX may lead to conflicting writes
   * - ``deps``
     - ``label_list``
     - false
     - ``[]``
     - List of labels to pass as deps attr to underlying lang_library rule
   * - ``alwayslink``
     - ``bool``
     - false
     - ``None``
     - Passed to the ``alwayslink`` attribute of ``cc_library``.
   * - ``copts``
     - ``string_list``
     - false
     - ``None``
     - Passed to the ``opts`` attribute of ``cc_library``.
   * - ``defines``
     - ``string_list``
     - false
     - ``None``
     - Passed to the ``defines`` attribute of ``cc_library``.
   * - ``include_prefix``
     - ``string``
     - false
     - ``None``
     - Passed to the ``include_prefix`` attribute of ``cc_library``.
   * - ``linkopts``
     - ``string_list``
     - false
     - ``None``
     - Passed to the ``linkopts`` attribute of ``cc_library``.
   * - ``linkstatic``
     - ``bool``
     - false
     - ``None``
     - Passed to the ``linkstatic`` attribute of ``cc_library``.
   * - ``local_defines``
     - ``string_list``
     - false
     - ``None``
     - Passed to the ``local_defines`` attribute of ``cc_library``.
   * - ``nocopts``
     - ``string``
     - false
     - ``None``
     - Passed to the ``nocopts`` attribute of ``cc_library``.
   * - ``strip_include_prefix``
     - ``string``
     - false
     - ``None``
     - Passed to the ``strip_include_prefix`` attribute of ``cc_library``.
