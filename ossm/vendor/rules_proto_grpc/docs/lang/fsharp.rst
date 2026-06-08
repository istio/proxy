:author: rules_proto_grpc
:description: rules_proto_grpc Bazel rules for F#
:keywords: Bazel, Protobuf, gRPC, Protocol Buffers, Rules, Build, Starlark, F#


F#
==

Rules for generating F# protobuf and gRPC ``.fs`` files and libraries using standard Protocol Buffers and gRPC. Libraries are created with ``fsharp_library`` from `rules_dotnet <https://github.com/bazelbuild/rules_dotnet>`_

.. list-table:: Rules
   :widths: 1 2
   :header-rows: 1

   * - Rule
     - Description
   * - `fsharp_proto_compile`_
     - Generates F# protobuf ``.fs`` files
   * - `fsharp_grpc_compile`_
     - Generates F# protobuf and gRPC ``.fs`` files
   * - `fsharp_proto_library`_
     - Generates a F# protobuf library using ``fsharp_library`` from ``rules_dotnet``. Note that the library name must end in ``.dll``
   * - `fsharp_grpc_library`_
     - Generates a F# protobuf and gRPC library using ``fsharp_library`` from ``rules_dotnet``. Note that the library name must end in ``.dll``

.. _fsharp_proto_compile:

fsharp_proto_compile
--------------------

Generates F# protobuf ``.fs`` files

Example
*******

Full example project can be found `here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/fsharp/fsharp_proto_compile>`__

``WORKSPACE``
^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//fsharp:repositories.bzl", rules_proto_grpc_fsharp_repos = "fsharp_repos")
   
   rules_proto_grpc_fsharp_repos()
   
   load("@io_bazel_rules_dotnet//dotnet:deps.bzl", "dotnet_repositories")
   
   dotnet_repositories()
   
   load(
       "@io_bazel_rules_dotnet//dotnet:defs.bzl",
       "dotnet_register_toolchains",
       "dotnet_repositories_nugets",
   )
   
   dotnet_register_toolchains()
   
   dotnet_repositories_nugets()
   
   load("@rules_proto_grpc//fsharp/nuget:nuget.bzl", "nuget_rules_proto_grpc_packages")
   
   nuget_rules_proto_grpc_packages()

``BUILD.bazel``
^^^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//fsharp:defs.bzl", "fsharp_proto_compile")
   
   fsharp_proto_compile(
       name = "person_fsharp_proto",
       protos = ["@rules_proto_grpc//example/proto:person_proto"],
   )
   
   fsharp_proto_compile(
       name = "place_fsharp_proto",
       protos = ["@rules_proto_grpc//example/proto:place_proto"],
   )
   
   fsharp_proto_compile(
       name = "thing_fsharp_proto",
       protos = ["@rules_proto_grpc//example/proto:thing_proto"],
   )

Attributes
**********

.. list-table:: Attributes for fsharp_proto_compile
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

- `@rules_proto_grpc//fsharp:fsharp_plugin <https://github.com/rules-proto-grpc/rules_proto_grpc/blob/master/fsharp/BUILD.bazel>`__

.. _fsharp_grpc_compile:

fsharp_grpc_compile
-------------------

Generates F# protobuf and gRPC ``.fs`` files

Example
*******

Full example project can be found `here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/fsharp/fsharp_grpc_compile>`__

``WORKSPACE``
^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//fsharp:repositories.bzl", rules_proto_grpc_fsharp_repos = "fsharp_repos")
   
   rules_proto_grpc_fsharp_repos()
   
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
   
   load("@rules_proto_grpc//fsharp/nuget:nuget.bzl", "nuget_rules_proto_grpc_packages")
   
   nuget_rules_proto_grpc_packages()

``BUILD.bazel``
^^^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//fsharp:defs.bzl", "fsharp_grpc_compile")
   
   fsharp_grpc_compile(
       name = "thing_fsharp_grpc",
       protos = ["@rules_proto_grpc//example/proto:thing_proto"],
   )
   
   fsharp_grpc_compile(
       name = "greeter_fsharp_grpc",
       protos = ["@rules_proto_grpc//example/proto:greeter_grpc"],
   )

Attributes
**********

.. list-table:: Attributes for fsharp_grpc_compile
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

- `@rules_proto_grpc//fsharp:grpc_fsharp_plugin <https://github.com/rules-proto-grpc/rules_proto_grpc/blob/master/fsharp/BUILD.bazel>`__

.. _fsharp_proto_library:

fsharp_proto_library
--------------------

Generates a F# protobuf library using ``fsharp_library`` from ``rules_dotnet``. Note that the library name must end in ``.dll``

Example
*******

Full example project can be found `here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/fsharp/fsharp_proto_library>`__

``WORKSPACE``
^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//fsharp:repositories.bzl", rules_proto_grpc_fsharp_repos = "fsharp_repos")
   
   rules_proto_grpc_fsharp_repos()
   
   load("@io_bazel_rules_dotnet//dotnet:deps.bzl", "dotnet_repositories")
   
   dotnet_repositories()
   
   load(
       "@io_bazel_rules_dotnet//dotnet:defs.bzl",
       "dotnet_register_toolchains",
       "dotnet_repositories_nugets",
   )
   
   dotnet_register_toolchains()
   
   dotnet_repositories_nugets()
   
   load("@rules_proto_grpc//fsharp/nuget:nuget.bzl", "nuget_rules_proto_grpc_packages")
   
   nuget_rules_proto_grpc_packages()

``BUILD.bazel``
^^^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//fsharp:defs.bzl", "fsharp_proto_library")
   
   fsharp_proto_library(
       name = "person_fsharp_proto.dll",
       protos = ["@rules_proto_grpc//example/proto:person_proto"],
       deps = ["place_fsharp_proto.dll"],
   )
   
   fsharp_proto_library(
       name = "place_fsharp_proto.dll",
       protos = ["@rules_proto_grpc//example/proto:place_proto"],
       deps = ["thing_fsharp_proto.dll"],
   )
   
   fsharp_proto_library(
       name = "thing_fsharp_proto.dll",
       protos = ["@rules_proto_grpc//example/proto:thing_proto"],
   )

Attributes
**********

.. list-table:: Attributes for fsharp_proto_library
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

.. _fsharp_grpc_library:

fsharp_grpc_library
-------------------

Generates a F# protobuf and gRPC library using ``fsharp_library`` from ``rules_dotnet``. Note that the library name must end in ``.dll``

Example
*******

Full example project can be found `here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/fsharp/fsharp_grpc_library>`__

``WORKSPACE``
^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//fsharp:repositories.bzl", rules_proto_grpc_fsharp_repos = "fsharp_repos")
   
   rules_proto_grpc_fsharp_repos()
   
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
   
   load("@rules_proto_grpc//fsharp/nuget:nuget.bzl", "nuget_rules_proto_grpc_packages")
   
   nuget_rules_proto_grpc_packages()

``BUILD.bazel``
^^^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//fsharp:defs.bzl", "fsharp_grpc_library")
   
   fsharp_grpc_library(
       name = "thing_fsharp_grpc.dll",
       protos = ["@rules_proto_grpc//example/proto:thing_proto"],
   )
   
   fsharp_grpc_library(
       name = "greeter_fsharp_grpc.dll",
       protos = ["@rules_proto_grpc//example/proto:greeter_grpc"],
       deps = ["thing_fsharp_grpc.dll"],
   )

Attributes
**********

.. list-table:: Attributes for fsharp_grpc_library
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
