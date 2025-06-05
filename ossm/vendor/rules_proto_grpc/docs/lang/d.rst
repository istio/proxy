:author: rules_proto_grpc
:description: rules_proto_grpc Bazel rules for D
:keywords: Bazel, Protobuf, gRPC, Protocol Buffers, Rules, Build, Starlark, D


D
=

Rules for generating D protobuf ``.d`` files and libraries using `protobuf-d <https://github.com/dcarp/protobuf-d>`_. Libraries are created with ``d_library`` from `rules_d <https://github.com/bazelbuild/rules_d>`_

.. note:: These rules use the protoc-gen-d plugin, which only supports proto3 .proto files.

.. list-table:: Rules
   :widths: 1 2
   :header-rows: 1

   * - Rule
     - Description
   * - `d_proto_compile`_
     - Generates D protobuf ``.d`` files
   * - `d_proto_library`_
     - Generates a D protobuf library using ``d_library`` from ``rules_d``

.. _d_proto_compile:

d_proto_compile
---------------

Generates D protobuf ``.d`` files

Example
*******

Full example project can be found `here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/d/d_proto_compile>`__

``WORKSPACE``
^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//d:repositories.bzl", rules_proto_grpc_d_repos = "d_repos")
   
   rules_proto_grpc_d_repos()
   
   load("@io_bazel_rules_d//d:d.bzl", "d_repositories")
   
   d_repositories()

``BUILD.bazel``
^^^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//d:defs.bzl", "d_proto_compile")
   
   d_proto_compile(
       name = "person_d_proto",
       protos = ["@rules_proto_grpc//example/proto:person_proto"],
   )
   
   d_proto_compile(
       name = "place_d_proto",
       protos = ["@rules_proto_grpc//example/proto:place_proto"],
   )
   
   d_proto_compile(
       name = "thing_d_proto",
       protos = ["@rules_proto_grpc//example/proto:thing_proto"],
   )

Attributes
**********

.. list-table:: Attributes for d_proto_compile
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

- `@rules_proto_grpc//d:d_plugin <https://github.com/rules-proto-grpc/rules_proto_grpc/blob/master/d/BUILD.bazel>`__

.. _d_proto_library:

d_proto_library
---------------

Generates a D protobuf library using ``d_library`` from ``rules_d``

Example
*******

Full example project can be found `here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/d/d_proto_library>`__

``WORKSPACE``
^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//d:repositories.bzl", rules_proto_grpc_d_repos = "d_repos")
   
   rules_proto_grpc_d_repos()
   
   load("@io_bazel_rules_d//d:d.bzl", "d_repositories")
   
   d_repositories()

``BUILD.bazel``
^^^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//d:defs.bzl", "d_proto_library")
   
   d_proto_library(
       name = "person_d_proto",
       protos = ["@rules_proto_grpc//example/proto:person_proto"],
       deps = ["place_d_proto"],
   )
   
   d_proto_library(
       name = "place_d_proto",
       protos = ["@rules_proto_grpc//example/proto:place_proto"],
       deps = ["thing_d_proto"],
   )
   
   d_proto_library(
       name = "thing_d_proto",
       protos = ["@rules_proto_grpc//example/proto:thing_proto"],
   )

Attributes
**********

.. list-table:: Attributes for d_proto_library
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
