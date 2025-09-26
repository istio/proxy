:author: rules_proto_grpc
:description: rules_proto_grpc Bazel rules for JavaScript
:keywords: Bazel, Protobuf, gRPC, Protocol Buffers, Rules, Build, Starlark, JavaScript


JavaScript
==========

Rules for generating JavaScript protobuf, gRPC-node and gRPC-Web ``.js`` and ``.d.ts`` files using standard Protocol Buffers and gRPC.

.. note:: You must add the required dependencies to your package.json file:

   .. code-block:: json

      "dependencies": {
        "@grpc/grpc-js": "1.7.3",
        "google-protobuf": "3.21.2",
        "grpc-tools": "1.11.3",
        "grpc-web": "1.4.2",
        "ts-protoc-gen": "0.15.0"
      }


.. list-table:: Rules
   :widths: 1 2
   :header-rows: 1

   * - Rule
     - Description
   * - `js_proto_compile`_
     - Generates JavaScript protobuf ``.js`` and ``.d.ts`` files
   * - `js_grpc_node_compile`_
     - Generates JavaScript protobuf and gRPC-node ``.js`` and ``.d.ts`` files
   * - `js_grpc_web_compile`_
     - Generates JavaScript protobuf and gRPC-Web ``.js`` and ``.d.ts`` files
   * - `js_proto_library`_
     - Generates a JavaScript protobuf library using ``js_library`` from ``rules_nodejs``
   * - `js_grpc_node_library`_
     - Generates a Node.js protobuf + gRPC-node library using ``js_library`` from ``rules_nodejs``
   * - `js_grpc_web_library`_
     - Generates a JavaScript protobuf + gRPC-Web library using ``js_library`` from ``rules_nodejs``

.. _js_proto_compile:

js_proto_compile
----------------

Generates JavaScript protobuf ``.js`` and ``.d.ts`` files

Example
*******

Full example project can be found `here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/js/js_proto_compile>`__

``WORKSPACE``
^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//js:repositories.bzl", rules_proto_grpc_js_repos = "js_repos")
   
   rules_proto_grpc_js_repos()
   
   load("@build_bazel_rules_nodejs//:repositories.bzl", "build_bazel_rules_nodejs_dependencies")
   
   build_bazel_rules_nodejs_dependencies()
   
   load("@build_bazel_rules_nodejs//:index.bzl", "yarn_install")
   
   yarn_install(
       name = "npm",
       package_json = "@rules_proto_grpc//js:requirements/package.json",  # This should be changed to your local package.json which should contain the dependencies required
       yarn_lock = "@rules_proto_grpc//js:requirements/yarn.lock",
   )

``BUILD.bazel``
^^^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//js:defs.bzl", "js_proto_compile")
   
   js_proto_compile(
       name = "person_js_proto",
       protos = ["@rules_proto_grpc//example/proto:person_proto"],
   )
   
   js_proto_compile(
       name = "place_js_proto",
       protos = ["@rules_proto_grpc//example/proto:place_proto"],
   )
   
   js_proto_compile(
       name = "thing_js_proto",
       protos = ["@rules_proto_grpc//example/proto:thing_proto"],
   )

Attributes
**********

.. list-table:: Attributes for js_proto_compile
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

- `@rules_proto_grpc//js:js_plugin <https://github.com/rules-proto-grpc/rules_proto_grpc/blob/master/js/BUILD.bazel>`__
- `@rules_proto_grpc//js:ts_plugin <https://github.com/rules-proto-grpc/rules_proto_grpc/blob/master/js/BUILD.bazel>`__

.. _js_grpc_node_compile:

js_grpc_node_compile
--------------------

Generates JavaScript protobuf and gRPC-node ``.js`` and ``.d.ts`` files

Example
*******

Full example project can be found `here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/js/js_grpc_node_compile>`__

``WORKSPACE``
^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//js:repositories.bzl", rules_proto_grpc_js_repos = "js_repos")
   
   rules_proto_grpc_js_repos()
   
   load("@build_bazel_rules_nodejs//:repositories.bzl", "build_bazel_rules_nodejs_dependencies")
   
   build_bazel_rules_nodejs_dependencies()
   
   load("@build_bazel_rules_nodejs//:index.bzl", "yarn_install")
   
   yarn_install(
       name = "npm",
       package_json = "@rules_proto_grpc//js:requirements/package.json",  # This should be changed to your local package.json which should contain the dependencies required
       yarn_lock = "@rules_proto_grpc//js:requirements/yarn.lock",
   )

``BUILD.bazel``
^^^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//js:defs.bzl", "js_grpc_node_compile")
   
   js_grpc_node_compile(
       name = "thing_js_grpc",
       protos = ["@rules_proto_grpc//example/proto:thing_proto"],
   )
   
   js_grpc_node_compile(
       name = "greeter_js_grpc",
       protos = ["@rules_proto_grpc//example/proto:greeter_grpc"],
   )

Attributes
**********

.. list-table:: Attributes for js_grpc_node_compile
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

- `@rules_proto_grpc//js:js_plugin <https://github.com/rules-proto-grpc/rules_proto_grpc/blob/master/js/BUILD.bazel>`__
- `@rules_proto_grpc//js:ts_plugin <https://github.com/rules-proto-grpc/rules_proto_grpc/blob/master/js/BUILD.bazel>`__
- `@rules_proto_grpc//js:grpc_node_plugin <https://github.com/rules-proto-grpc/rules_proto_grpc/blob/master/js/BUILD.bazel>`__
- `@rules_proto_grpc//js:grpc_node_ts_plugin <https://github.com/rules-proto-grpc/rules_proto_grpc/blob/master/js/BUILD.bazel>`__

.. _js_grpc_web_compile:

js_grpc_web_compile
-------------------

Generates JavaScript protobuf and gRPC-Web ``.js`` and ``.d.ts`` files

Example
*******

Full example project can be found `here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/js/js_grpc_web_compile>`__

``WORKSPACE``
^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//js:repositories.bzl", rules_proto_grpc_js_repos = "js_repos")
   
   rules_proto_grpc_js_repos()
   
   load("@build_bazel_rules_nodejs//:repositories.bzl", "build_bazel_rules_nodejs_dependencies")
   
   build_bazel_rules_nodejs_dependencies()
   
   load("@build_bazel_rules_nodejs//:index.bzl", "yarn_install")
   
   yarn_install(
       name = "npm",
       package_json = "@rules_proto_grpc//js:requirements/package.json",  # This should be changed to your local package.json which should contain the dependencies required
       yarn_lock = "@rules_proto_grpc//js:requirements/yarn.lock",
   )

``BUILD.bazel``
^^^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//js:defs.bzl", "js_grpc_web_compile")
   
   js_grpc_web_compile(
       name = "thing_js_grpc",
       protos = ["@rules_proto_grpc//example/proto:thing_proto"],
   )
   
   js_grpc_web_compile(
       name = "greeter_js_grpc",
       protos = ["@rules_proto_grpc//example/proto:greeter_grpc"],
   )

Attributes
**********

.. list-table:: Attributes for js_grpc_web_compile
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

- `@rules_proto_grpc//js:js_plugin <https://github.com/rules-proto-grpc/rules_proto_grpc/blob/master/js/BUILD.bazel>`__
- `@rules_proto_grpc//js:ts_plugin <https://github.com/rules-proto-grpc/rules_proto_grpc/blob/master/js/BUILD.bazel>`__
- `@rules_proto_grpc//js:grpc_web_js_plugin <https://github.com/rules-proto-grpc/rules_proto_grpc/blob/master/js/BUILD.bazel>`__

.. _js_proto_library:

js_proto_library
----------------

Generates a JavaScript protobuf library using ``js_library`` from ``rules_nodejs``

Example
*******

Full example project can be found `here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/js/js_proto_library>`__

``WORKSPACE``
^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//js:repositories.bzl", rules_proto_grpc_js_repos = "js_repos")
   
   rules_proto_grpc_js_repos()
   
   load("@build_bazel_rules_nodejs//:repositories.bzl", "build_bazel_rules_nodejs_dependencies")
   
   build_bazel_rules_nodejs_dependencies()
   
   load("@build_bazel_rules_nodejs//:index.bzl", "yarn_install")
   
   yarn_install(
       name = "npm",
       package_json = "@rules_proto_grpc//js:requirements/package.json",  # This should be changed to your local package.json which should contain the dependencies required
       yarn_lock = "@rules_proto_grpc//js:requirements/yarn.lock",
   )

``BUILD.bazel``
^^^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//js:defs.bzl", "js_proto_library")
   
   js_proto_library(
       name = "person_js_proto",
       protos = ["@rules_proto_grpc//example/proto:person_proto"],
       deps = ["place_js_proto"],
   )
   
   js_proto_library(
       name = "place_js_proto",
       protos = ["@rules_proto_grpc//example/proto:place_proto"],
       deps = ["thing_js_proto"],
   )
   
   js_proto_library(
       name = "thing_js_proto",
       protos = ["@rules_proto_grpc//example/proto:thing_proto"],
   )

Attributes
**********

.. list-table:: Attributes for js_proto_library
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
   * - ``package_name``
     - ``string``
     - false
     - 
     - The package name to use for the library. If unprovided, the target name is used.
   * - ``deps_repo``
     - ``string``
     - false
     - ``@npm``
     - The repository to load the dependencies from, if you don't use ``@npm``
   * - ``legacy_path``
     - ``bool``
     - false
     - ``False``
     - Use the legacy <name>_pb path segment from the generated library require path.

.. _js_grpc_node_library:

js_grpc_node_library
--------------------

Generates a Node.js protobuf + gRPC-node library using ``js_library`` from ``rules_nodejs``

Example
*******

Full example project can be found `here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/js/js_grpc_node_library>`__

``WORKSPACE``
^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//js:repositories.bzl", rules_proto_grpc_js_repos = "js_repos")
   
   rules_proto_grpc_js_repos()
   
   load("@build_bazel_rules_nodejs//:repositories.bzl", "build_bazel_rules_nodejs_dependencies")
   
   build_bazel_rules_nodejs_dependencies()
   
   load("@build_bazel_rules_nodejs//:index.bzl", "yarn_install")
   
   yarn_install(
       name = "npm",
       package_json = "@rules_proto_grpc//js:requirements/package.json",  # This should be changed to your local package.json which should contain the dependencies required
       yarn_lock = "@rules_proto_grpc//js:requirements/yarn.lock",
   )

``BUILD.bazel``
^^^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//js:defs.bzl", "js_grpc_node_library")
   
   js_grpc_node_library(
       name = "thing_js_grpc",
       protos = ["@rules_proto_grpc//example/proto:thing_proto"],
   )
   
   js_grpc_node_library(
       name = "greeter_js_grpc",
       protos = ["@rules_proto_grpc//example/proto:greeter_grpc"],
       deps = ["thing_js_grpc"],
   )

Attributes
**********

.. list-table:: Attributes for js_grpc_node_library
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
   * - ``package_name``
     - ``string``
     - false
     - 
     - The package name to use for the library. If unprovided, the target name is used.
   * - ``deps_repo``
     - ``string``
     - false
     - ``@npm``
     - The repository to load the dependencies from, if you don't use ``@npm``
   * - ``legacy_path``
     - ``bool``
     - false
     - ``False``
     - Use the legacy <name>_pb path segment from the generated library require path.

.. _js_grpc_web_library:

js_grpc_web_library
-------------------

Generates a JavaScript protobuf + gRPC-Web library using ``js_library`` from ``rules_nodejs``

Example
*******

Full example project can be found `here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/js/js_grpc_web_library>`__

``WORKSPACE``
^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//js:repositories.bzl", rules_proto_grpc_js_repos = "js_repos")
   
   rules_proto_grpc_js_repos()
   
   load("@build_bazel_rules_nodejs//:repositories.bzl", "build_bazel_rules_nodejs_dependencies")
   
   build_bazel_rules_nodejs_dependencies()
   
   load("@build_bazel_rules_nodejs//:index.bzl", "yarn_install")
   
   yarn_install(
       name = "npm",
       package_json = "@rules_proto_grpc//js:requirements/package.json",  # This should be changed to your local package.json which should contain the dependencies required
       yarn_lock = "@rules_proto_grpc//js:requirements/yarn.lock",
   )

``BUILD.bazel``
^^^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//js:defs.bzl", "js_grpc_web_library")
   
   js_grpc_web_library(
       name = "thing_js_grpc",
       protos = ["@rules_proto_grpc//example/proto:thing_proto"],
   )
   
   js_grpc_web_library(
       name = "greeter_js_grpc",
       protos = ["@rules_proto_grpc//example/proto:greeter_grpc"],
       deps = ["thing_js_grpc"],
   )

Attributes
**********

.. list-table:: Attributes for js_grpc_web_library
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
   * - ``package_name``
     - ``string``
     - false
     - 
     - The package name to use for the library. If unprovided, the target name is used.
   * - ``deps_repo``
     - ``string``
     - false
     - ``@npm``
     - The repository to load the dependencies from, if you don't use ``@npm``
   * - ``legacy_path``
     - ``bool``
     - false
     - ``False``
     - Use the legacy <name>_pb path segment from the generated library require path.
