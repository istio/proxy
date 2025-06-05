:author: rules_proto_grpc
:description: rules_proto_grpc Bazel rules for Go
:keywords: Bazel, Protobuf, gRPC, Protocol Buffers, Rules, Build, Starlark, Go


Go
==

Rules for generating Go protobuf and gRPC ``.go`` files and libraries using `golang/protobuf <https://github.com/golang/protobuf>`_. Libraries are created with ``go_library`` from `rules_go <https://github.com/bazelbuild/rules_go>`_

.. list-table:: Rules
   :widths: 1 2
   :header-rows: 1

   * - Rule
     - Description
   * - `go_proto_compile`_
     - Generates Go protobuf ``.go`` files
   * - `go_grpc_compile`_
     - Generates Go protobuf and gRPC ``.go`` files
   * - `go_validate_compile`_
     - Generates Go protobuf and gRPC validation ``.go`` files
   * - `go_proto_library`_
     - Generates a Go protobuf library using ``go_library`` from ``rules_go``
   * - `go_grpc_library`_
     - Generates a Go protobuf and gRPC library using ``go_library`` from ``rules_go``
   * - `go_validate_library`_
     - Generates a Go protobuf and gRPC validation library using ``go_library`` from ``rules_go``

.. _go_proto_compile:

go_proto_compile
----------------

Generates Go protobuf ``.go`` files

Example
*******

Full example project can be found `here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/go/go_proto_compile>`__

``WORKSPACE``
^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//:repositories.bzl", "bazel_gazelle", "io_bazel_rules_go")  # buildifier: disable=same-origin-load
   
   io_bazel_rules_go()
   
   bazel_gazelle()
   
   load("@rules_proto_grpc//go:repositories.bzl", rules_proto_grpc_go_repos = "go_repos")
   
   rules_proto_grpc_go_repos()
   
   load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")
   
   go_rules_dependencies()
   
   go_register_toolchains(
       version = "1.17.1",
   )
   
   load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies")
   
   gazelle_dependencies()

``BUILD.bazel``
^^^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//go:defs.bzl", "go_proto_compile")
   
   go_proto_compile(
       name = "person_go_proto",
       protos = ["@rules_proto_grpc//example/proto:person_proto"],
   )
   
   go_proto_compile(
       name = "place_go_proto",
       protos = ["@rules_proto_grpc//example/proto:place_proto"],
   )
   
   go_proto_compile(
       name = "thing_go_proto",
       protos = ["@rules_proto_grpc//example/proto:thing_proto"],
   )

Attributes
**********

.. list-table:: Attributes for go_proto_compile
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

- `@rules_proto_grpc//go:go_plugin <https://github.com/rules-proto-grpc/rules_proto_grpc/blob/master/go/BUILD.bazel>`__

.. _go_grpc_compile:

go_grpc_compile
---------------

Generates Go protobuf and gRPC ``.go`` files

Example
*******

Full example project can be found `here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/go/go_grpc_compile>`__

``WORKSPACE``
^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//:repositories.bzl", "bazel_gazelle", "io_bazel_rules_go")  # buildifier: disable=same-origin-load
   
   io_bazel_rules_go()
   
   bazel_gazelle()
   
   load("@rules_proto_grpc//go:repositories.bzl", rules_proto_grpc_go_repos = "go_repos")
   
   rules_proto_grpc_go_repos()
   
   load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")
   
   go_rules_dependencies()
   
   go_register_toolchains(
       version = "1.17.1",
   )
   
   load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies")
   
   gazelle_dependencies()

``BUILD.bazel``
^^^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//go:defs.bzl", "go_grpc_compile")
   
   go_grpc_compile(
       name = "thing_go_grpc",
       protos = ["@rules_proto_grpc//example/proto:thing_proto"],
   )
   
   go_grpc_compile(
       name = "greeter_go_grpc",
       protos = ["@rules_proto_grpc//example/proto:greeter_grpc"],
   )

Attributes
**********

.. list-table:: Attributes for go_grpc_compile
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

- `@rules_proto_grpc//go:go_plugin <https://github.com/rules-proto-grpc/rules_proto_grpc/blob/master/go/BUILD.bazel>`__
- `@rules_proto_grpc//go:grpc_go_plugin <https://github.com/rules-proto-grpc/rules_proto_grpc/blob/master/go/BUILD.bazel>`__

.. _go_validate_compile:

go_validate_compile
-------------------

.. warning:: This rule is experimental. It may not work correctly or may change in future releases!

Generates Go protobuf and gRPC validation ``.go`` files

Example
*******

Full example project can be found `here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/go/go_validate_compile>`__

``WORKSPACE``
^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//:repositories.bzl", "bazel_gazelle", "io_bazel_rules_go")  # buildifier: disable=same-origin-load
   
   io_bazel_rules_go()
   
   bazel_gazelle()
   
   load("@rules_proto_grpc//go:repositories.bzl", rules_proto_grpc_go_repos = "go_repos")
   
   rules_proto_grpc_go_repos()
   
   load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")
   
   go_rules_dependencies()
   
   go_register_toolchains(
       version = "1.17.1",
   )
   
   load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies")
   
   gazelle_dependencies()

``BUILD.bazel``
^^^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//go:defs.bzl", "go_validate_compile")
   
   go_validate_compile(
       name = "thing_go_validate",
       protos = ["@rules_proto_grpc//example/proto:thing_proto"],
   )
   
   go_validate_compile(
       name = "greeter_go_validate",
       protos = ["@rules_proto_grpc//example/proto:greeter_grpc"],
   )

Attributes
**********

.. list-table:: Attributes for go_validate_compile
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

- `@rules_proto_grpc//go:go_plugin <https://github.com/rules-proto-grpc/rules_proto_grpc/blob/master/go/BUILD.bazel>`__
- `@rules_proto_grpc//go:grpc_go_plugin <https://github.com/rules-proto-grpc/rules_proto_grpc/blob/master/go/BUILD.bazel>`__
- `@rules_proto_grpc//go:validate_go_plugin <https://github.com/rules-proto-grpc/rules_proto_grpc/blob/master/go/BUILD.bazel>`__

.. _go_proto_library:

go_proto_library
----------------

Generates a Go protobuf library using ``go_library`` from ``rules_go``

Example
*******

Full example project can be found `here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/go/go_proto_library>`__

``WORKSPACE``
^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//:repositories.bzl", "bazel_gazelle", "io_bazel_rules_go")  # buildifier: disable=same-origin-load
   
   io_bazel_rules_go()
   
   bazel_gazelle()
   
   load("@rules_proto_grpc//go:repositories.bzl", rules_proto_grpc_go_repos = "go_repos")
   
   rules_proto_grpc_go_repos()
   
   load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")
   
   go_rules_dependencies()
   
   go_register_toolchains(
       version = "1.17.1",
   )
   
   load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies")
   
   gazelle_dependencies()

``BUILD.bazel``
^^^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//go:defs.bzl", "go_proto_library")
   
   go_proto_library(
       name = "proto_go_proto",
       importpath = "github.com/rules-proto-grpc/rules_proto_grpc/example/proto",
       protos = [
           "@rules_proto_grpc//example/proto:person_proto",
           "@rules_proto_grpc//example/proto:place_proto",
           "@rules_proto_grpc//example/proto:thing_proto",
       ],
   )

Attributes
**********

.. list-table:: Attributes for go_proto_library
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
   * - ``importpath``
     - ``string``
     - false
     - ``None``
     - Importpath for the generated files

.. _go_grpc_library:

go_grpc_library
---------------

Generates a Go protobuf and gRPC library using ``go_library`` from ``rules_go``

Example
*******

Full example project can be found `here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/go/go_grpc_library>`__

``WORKSPACE``
^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//:repositories.bzl", "bazel_gazelle", "io_bazel_rules_go")  # buildifier: disable=same-origin-load
   
   io_bazel_rules_go()
   
   bazel_gazelle()
   
   load("@rules_proto_grpc//go:repositories.bzl", rules_proto_grpc_go_repos = "go_repos")
   
   rules_proto_grpc_go_repos()
   
   load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")
   
   go_rules_dependencies()
   
   go_register_toolchains(
       version = "1.17.1",
   )
   
   load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies")
   
   gazelle_dependencies()

``BUILD.bazel``
^^^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//go:defs.bzl", "go_grpc_library")
   
   go_grpc_library(
       name = "greeter_go_grpc",
       importpath = "github.com/rules-proto-grpc/rules_proto_grpc/example/proto",
       protos = [
           "@rules_proto_grpc//example/proto:greeter_grpc",
           "@rules_proto_grpc//example/proto:thing_proto",
       ],
   )

Attributes
**********

.. list-table:: Attributes for go_grpc_library
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
   * - ``importpath``
     - ``string``
     - false
     - ``None``
     - Importpath for the generated files

.. _go_validate_library:

go_validate_library
-------------------

.. warning:: This rule is experimental. It may not work correctly or may change in future releases!

Generates a Go protobuf and gRPC validation library using ``go_library`` from ``rules_go``

Example
*******

Full example project can be found `here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/go/go_validate_library>`__

``WORKSPACE``
^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//:repositories.bzl", "bazel_gazelle", "io_bazel_rules_go")  # buildifier: disable=same-origin-load
   
   io_bazel_rules_go()
   
   bazel_gazelle()
   
   load("@rules_proto_grpc//go:repositories.bzl", rules_proto_grpc_go_repos = "go_repos")
   
   rules_proto_grpc_go_repos()
   
   load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")
   
   go_rules_dependencies()
   
   go_register_toolchains(
       version = "1.17.1",
   )
   
   load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies")
   
   gazelle_dependencies()

``BUILD.bazel``
^^^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//go:defs.bzl", "go_validate_library")
   
   go_validate_library(
       name = "greeter_go_validate",
       importpath = "github.com/rules-proto-grpc/rules_proto_grpc/example/proto",
       protos = [
           "@rules_proto_grpc//example/proto:greeter_grpc",
           "@rules_proto_grpc//example/proto:thing_proto",
       ],
   )

Attributes
**********

.. list-table:: Attributes for go_validate_library
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
   * - ``importpath``
     - ``string``
     - false
     - ``None``
     - Importpath for the generated files
