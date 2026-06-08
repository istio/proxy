:author: rules_proto_grpc
:description: rules_proto_grpc Bazel rules for C#
:keywords: Bazel, Protobuf, gRPC, Protocol Buffers, Rules, Build, Starlark, C#


C#
==

Rules for generating C# protobuf and gRPC ``.cs`` files and libraries using standard Protocol Buffers and gRPC. Libraries are created with ``csharp_library`` from `rules_dotnet <https://github.com/bazelbuild/rules_dotnet>`_

.. list-table:: Rules
   :widths: 1 2
   :header-rows: 1

   * - Rule
     - Description
   * - `csharp_proto_compile`_
     - Generates C# protobuf ``.cs`` files
   * - `csharp_grpc_compile`_
     - Generates C# protobuf and gRPC ``.cs`` files
   * - `csharp_proto_library`_
     - Generates a C# protobuf library using ``csharp_library`` from ``rules_dotnet``. Note that the library name must end in ``.dll``
   * - `csharp_grpc_library`_
     - Generates a C# protobuf and gRPC library using ``csharp_library`` from ``rules_dotnet``. Note that the library name must end in ``.dll``

.. _csharp_proto_compile:

csharp_proto_compile
--------------------

Generates C# protobuf ``.cs`` files

Example
*******

Full example project can be found `here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/csharp/csharp_proto_compile>`__

``WORKSPACE``
^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//csharp:repositories.bzl", rules_proto_grpc_csharp_repos = "csharp_repos")
   
   rules_proto_grpc_csharp_repos()
   
   load("@io_bazel_rules_dotnet//dotnet:deps.bzl", "dotnet_repositories")
   
   dotnet_repositories()
   
   load(
       "@io_bazel_rules_dotnet//dotnet:defs.bzl",
       "dotnet_register_toolchains",
       "dotnet_repositories_nugets",
   )
   
   dotnet_register_toolchains()
   
   dotnet_repositories_nugets()
   
   load("@rules_proto_grpc//csharp/nuget:nuget.bzl", "nuget_rules_proto_grpc_packages")
   
   nuget_rules_proto_grpc_packages()

``BUILD.bazel``
^^^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//csharp:defs.bzl", "csharp_proto_compile")
   
   csharp_proto_compile(
       name = "person_csharp_proto",
       protos = ["@rules_proto_grpc//example/proto:person_proto"],
   )
   
   csharp_proto_compile(
       name = "place_csharp_proto",
       protos = ["@rules_proto_grpc//example/proto:place_proto"],
   )
   
   csharp_proto_compile(
       name = "thing_csharp_proto",
       protos = ["@rules_proto_grpc//example/proto:thing_proto"],
   )

Attributes
**********

.. list-table:: Attributes for csharp_proto_compile
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

- `@rules_proto_grpc//csharp:csharp_plugin <https://github.com/rules-proto-grpc/rules_proto_grpc/blob/master/csharp/BUILD.bazel>`__

.. _csharp_grpc_compile:

csharp_grpc_compile
-------------------

Generates C# protobuf and gRPC ``.cs`` files

Example
*******

Full example project can be found `here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/csharp/csharp_grpc_compile>`__

``WORKSPACE``
^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//csharp:repositories.bzl", rules_proto_grpc_csharp_repos = "csharp_repos")
   
   rules_proto_grpc_csharp_repos()
   
   load("@io_bazel_rules_dotnet//dotnet:deps.bzl", "dotnet_repositories")
   load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")
   
   grpc_deps()
   
   dotnet_repositories()
   
   load(
       "@io_bazel_rules_dotnet//dotnet:defs.bzl",
       "dotnet_register_toolchains",
       "dotnet_repositories_nugets",
   )
   
   dotnet_register_toolchains()
   
   dotnet_repositories_nugets()
   
   load("@rules_proto_grpc//csharp/nuget:nuget.bzl", "nuget_rules_proto_grpc_packages")
   
   nuget_rules_proto_grpc_packages()

``BUILD.bazel``
^^^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//csharp:defs.bzl", "csharp_grpc_compile")
   
   csharp_grpc_compile(
       name = "thing_csharp_grpc",
       protos = ["@rules_proto_grpc//example/proto:thing_proto"],
   )
   
   csharp_grpc_compile(
       name = "greeter_csharp_grpc",
       protos = ["@rules_proto_grpc//example/proto:greeter_grpc"],
   )

Attributes
**********

.. list-table:: Attributes for csharp_grpc_compile
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

- `@rules_proto_grpc//csharp:csharp_plugin <https://github.com/rules-proto-grpc/rules_proto_grpc/blob/master/csharp/BUILD.bazel>`__
- `@rules_proto_grpc//csharp:grpc_csharp_plugin <https://github.com/rules-proto-grpc/rules_proto_grpc/blob/master/csharp/BUILD.bazel>`__

.. _csharp_proto_library:

csharp_proto_library
--------------------

Generates a C# protobuf library using ``csharp_library`` from ``rules_dotnet``. Note that the library name must end in ``.dll``

Example
*******

Full example project can be found `here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/csharp/csharp_proto_library>`__

``WORKSPACE``
^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//csharp:repositories.bzl", rules_proto_grpc_csharp_repos = "csharp_repos")
   
   rules_proto_grpc_csharp_repos()
   
   load("@io_bazel_rules_dotnet//dotnet:deps.bzl", "dotnet_repositories")
   
   dotnet_repositories()
   
   load(
       "@io_bazel_rules_dotnet//dotnet:defs.bzl",
       "dotnet_register_toolchains",
       "dotnet_repositories_nugets",
   )
   
   dotnet_register_toolchains()
   
   dotnet_repositories_nugets()
   
   load("@rules_proto_grpc//csharp/nuget:nuget.bzl", "nuget_rules_proto_grpc_packages")
   
   nuget_rules_proto_grpc_packages()

``BUILD.bazel``
^^^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//csharp:defs.bzl", "csharp_proto_library")
   
   csharp_proto_library(
       name = "person_csharp_proto.dll",
       protos = ["@rules_proto_grpc//example/proto:person_proto"],
       deps = ["place_csharp_proto.dll"],
   )
   
   csharp_proto_library(
       name = "place_csharp_proto.dll",
       protos = ["@rules_proto_grpc//example/proto:place_proto"],
       deps = ["thing_csharp_proto.dll"],
   )
   
   csharp_proto_library(
       name = "thing_csharp_proto.dll",
       protos = ["@rules_proto_grpc//example/proto:thing_proto"],
   )

Attributes
**********

.. list-table:: Attributes for csharp_proto_library
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

.. _csharp_grpc_library:

csharp_grpc_library
-------------------

Generates a C# protobuf and gRPC library using ``csharp_library`` from ``rules_dotnet``. Note that the library name must end in ``.dll``

Example
*******

Full example project can be found `here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/csharp/csharp_grpc_library>`__

``WORKSPACE``
^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//csharp:repositories.bzl", rules_proto_grpc_csharp_repos = "csharp_repos")
   
   rules_proto_grpc_csharp_repos()
   
   load("@io_bazel_rules_dotnet//dotnet:deps.bzl", "dotnet_repositories")
   load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")
   
   grpc_deps()
   
   dotnet_repositories()
   
   load(
       "@io_bazel_rules_dotnet//dotnet:defs.bzl",
       "dotnet_register_toolchains",
       "dotnet_repositories_nugets",
   )
   
   dotnet_register_toolchains()
   
   dotnet_repositories_nugets()
   
   load("@rules_proto_grpc//csharp/nuget:nuget.bzl", "nuget_rules_proto_grpc_packages")
   
   nuget_rules_proto_grpc_packages()

``BUILD.bazel``
^^^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//csharp:defs.bzl", "csharp_grpc_library")
   
   csharp_grpc_library(
       name = "thing_csharp_grpc.dll",
       protos = ["@rules_proto_grpc//example/proto:thing_proto"],
   )
   
   csharp_grpc_library(
       name = "greeter_csharp_grpc.dll",
       protos = ["@rules_proto_grpc//example/proto:greeter_grpc"],
       deps = ["thing_csharp_grpc.dll"],
   )

Attributes
**********

.. list-table:: Attributes for csharp_grpc_library
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
