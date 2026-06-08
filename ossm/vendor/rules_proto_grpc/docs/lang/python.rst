:author: rules_proto_grpc
:description: rules_proto_grpc Bazel rules for Python
:keywords: Bazel, Protobuf, gRPC, Protocol Buffers, Rules, Build, Starlark, Python


Python
======

Rules for generating Python protobuf and gRPC ``.py`` files and libraries using standard Protocol Buffers and gRPC or `grpclib <https://github.com/vmagamedov/grpclib>`_. Libraries are created with ``py_library`` from ``rules_python``. To use the fast C++ Protobuf implementation, you can add ``--define=use_fast_cpp_protos=true`` to your build, but this requires you setup the path to your Python headers.

.. note:: On Windows, the path to Python for ``pip_install`` may need updating to ``Python.exe``, depending on your install.

.. note:: If you have proto libraries that produce overlapping import paths, be sure to set ``legacy_create_init=False`` on the top level ``py_binary`` or ``py_test`` to ensure all paths are importable.

.. list-table:: Rules
   :widths: 1 2
   :header-rows: 1

   * - Rule
     - Description
   * - `python_proto_compile`_
     - Generates Python protobuf ``.py`` files
   * - `python_grpc_compile`_
     - Generates Python protobuf and gRPC ``.py`` files
   * - `python_grpclib_compile`_
     - Generates Python protobuf and grpclib ``.py`` files (supports Python 3 only)
   * - `python_proto_library`_
     - Generates a Python protobuf library using ``py_library`` from ``rules_python``
   * - `python_grpc_library`_
     - Generates a Python protobuf and gRPC library using ``py_library`` from ``rules_python``
   * - `python_grpclib_library`_
     - Generates a Python protobuf and grpclib library using ``py_library`` from ``rules_python`` (supports Python 3 only)

.. _python_proto_compile:

python_proto_compile
--------------------

Generates Python protobuf ``.py`` files

Example
*******

Full example project can be found `here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/python/python_proto_compile>`__

``WORKSPACE``
^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//python:repositories.bzl", rules_proto_grpc_python_repos = "python_repos")
   
   rules_proto_grpc_python_repos()

``BUILD.bazel``
^^^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//python:defs.bzl", "python_proto_compile")
   
   python_proto_compile(
       name = "person_python_proto",
       protos = ["@rules_proto_grpc//example/proto:person_proto"],
   )
   
   python_proto_compile(
       name = "place_python_proto",
       protos = ["@rules_proto_grpc//example/proto:place_proto"],
   )
   
   python_proto_compile(
       name = "thing_python_proto",
       protos = ["@rules_proto_grpc//example/proto:thing_proto"],
   )

Attributes
**********

.. list-table:: Attributes for python_proto_compile
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

- `@rules_proto_grpc//python:python_plugin <https://github.com/rules-proto-grpc/rules_proto_grpc/blob/master/python/BUILD.bazel>`__

.. _python_grpc_compile:

python_grpc_compile
-------------------

Generates Python protobuf and gRPC ``.py`` files

Example
*******

Full example project can be found `here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/python/python_grpc_compile>`__

``WORKSPACE``
^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//python:repositories.bzl", rules_proto_grpc_python_repos = "python_repos")
   
   rules_proto_grpc_python_repos()
   
   load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")
   
   grpc_deps()
   
   load("@com_github_grpc_grpc//bazel:grpc_extra_deps.bzl", "grpc_extra_deps")
   
   grpc_extra_deps()

``BUILD.bazel``
^^^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//python:defs.bzl", "python_grpc_compile")
   
   python_grpc_compile(
       name = "thing_python_grpc",
       protos = ["@rules_proto_grpc//example/proto:thing_proto"],
   )
   
   python_grpc_compile(
       name = "greeter_python_grpc",
       protos = ["@rules_proto_grpc//example/proto:greeter_grpc"],
   )

Attributes
**********

.. list-table:: Attributes for python_grpc_compile
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

- `@rules_proto_grpc//python:python_plugin <https://github.com/rules-proto-grpc/rules_proto_grpc/blob/master/python/BUILD.bazel>`__
- `@rules_proto_grpc//python:grpc_python_plugin <https://github.com/rules-proto-grpc/rules_proto_grpc/blob/master/python/BUILD.bazel>`__

.. _python_grpclib_compile:

python_grpclib_compile
----------------------

Generates Python protobuf and grpclib ``.py`` files (supports Python 3 only)

Example
*******

Full example project can be found `here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/python/python_grpclib_compile>`__

``WORKSPACE``
^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//python:repositories.bzl", rules_proto_grpc_python_repos = "python_repos")
   
   rules_proto_grpc_python_repos()
   
   load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")
   
   grpc_deps()
   
   load("@com_github_grpc_grpc//bazel:grpc_extra_deps.bzl", "grpc_extra_deps")
   
   grpc_extra_deps()
   
   load("@rules_python//python:pip.bzl", "pip_parse")
   
   pip_parse(
       name = "rules_proto_grpc_py3_deps",
       python_interpreter = "python3",
       requirements_lock = "@rules_proto_grpc//python:requirements.txt",
   )
   
   load("@rules_proto_grpc_py3_deps//:requirements.bzl", "install_deps")
   
   install_deps()

``BUILD.bazel``
^^^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//python:defs.bzl", "python_grpclib_compile")
   
   python_grpclib_compile(
       name = "thing_python_grpc",
       protos = ["@rules_proto_grpc//example/proto:thing_proto"],
   )
   
   python_grpclib_compile(
       name = "greeter_python_grpc",
       protos = ["@rules_proto_grpc//example/proto:greeter_grpc"],
   )

Attributes
**********

.. list-table:: Attributes for python_grpclib_compile
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

- `@rules_proto_grpc//python:python_plugin <https://github.com/rules-proto-grpc/rules_proto_grpc/blob/master/python/BUILD.bazel>`__
- `@rules_proto_grpc//python:grpclib_python_plugin <https://github.com/rules-proto-grpc/rules_proto_grpc/blob/master/python/BUILD.bazel>`__

.. _python_proto_library:

python_proto_library
--------------------

Generates a Python protobuf library using ``py_library`` from ``rules_python``

Example
*******

Full example project can be found `here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/python/python_proto_library>`__

``WORKSPACE``
^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//python:repositories.bzl", rules_proto_grpc_python_repos = "python_repos")
   
   rules_proto_grpc_python_repos()

``BUILD.bazel``
^^^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//python:defs.bzl", "python_proto_library")
   
   python_proto_library(
       name = "person_python_proto",
       protos = ["@rules_proto_grpc//example/proto:person_proto"],
       deps = ["place_python_proto"],
   )
   
   python_proto_library(
       name = "place_python_proto",
       protos = ["@rules_proto_grpc//example/proto:place_proto"],
       deps = ["thing_python_proto"],
   )
   
   python_proto_library(
       name = "thing_python_proto",
       protos = ["@rules_proto_grpc//example/proto:thing_proto"],
   )

Attributes
**********

.. list-table:: Attributes for python_proto_library
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

.. _python_grpc_library:

python_grpc_library
-------------------

Generates a Python protobuf and gRPC library using ``py_library`` from ``rules_python``

Example
*******

Full example project can be found `here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/python/python_grpc_library>`__

``WORKSPACE``
^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//python:repositories.bzl", rules_proto_grpc_python_repos = "python_repos")
   
   rules_proto_grpc_python_repos()
   
   load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")
   
   grpc_deps()
   
   load("@com_github_grpc_grpc//bazel:grpc_extra_deps.bzl", "grpc_extra_deps")
   
   grpc_extra_deps()

``BUILD.bazel``
^^^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//python:defs.bzl", "python_grpc_library")
   
   python_grpc_library(
       name = "thing_python_grpc",
       protos = ["@rules_proto_grpc//example/proto:thing_proto"],
   )
   
   python_grpc_library(
       name = "greeter_python_grpc",
       protos = ["@rules_proto_grpc//example/proto:greeter_grpc"],
       deps = ["thing_python_grpc"],
   )

Attributes
**********

.. list-table:: Attributes for python_grpc_library
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

.. _python_grpclib_library:

python_grpclib_library
----------------------

Generates a Python protobuf and grpclib library using ``py_library`` from ``rules_python`` (supports Python 3 only)

Example
*******

Full example project can be found `here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/python/python_grpclib_library>`__

``WORKSPACE``
^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//python:repositories.bzl", rules_proto_grpc_python_repos = "python_repos")
   
   rules_proto_grpc_python_repos()
   
   load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")
   
   grpc_deps()
   
   load("@com_github_grpc_grpc//bazel:grpc_extra_deps.bzl", "grpc_extra_deps")
   
   grpc_extra_deps()
   
   load("@rules_python//python:pip.bzl", "pip_parse")
   
   pip_parse(
       name = "rules_proto_grpc_py3_deps",
       python_interpreter = "python3",
       requirements_lock = "@rules_proto_grpc//python:requirements.txt",
   )
   
   load("@rules_proto_grpc_py3_deps//:requirements.bzl", "install_deps")
   
   install_deps()

``BUILD.bazel``
^^^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//python:defs.bzl", "python_grpclib_library")
   
   python_grpclib_library(
       name = "thing_python_grpc",
       protos = ["@rules_proto_grpc//example/proto:thing_proto"],
   )
   
   python_grpclib_library(
       name = "greeter_python_grpc",
       protos = ["@rules_proto_grpc//example/proto:greeter_grpc"],
       deps = ["thing_python_grpc"],
   )

Attributes
**********

.. list-table:: Attributes for python_grpclib_library
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
