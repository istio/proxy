:author: rules_proto_grpc
:description: rules_proto_grpc Bazel rules for Scala
:keywords: Bazel, Protobuf, gRPC, Protocol Buffers, Rules, Build, Starlark, Scala


Scala
=====

Rules for generating Scala protobuf and gRPC ``.jar`` files and libraries using `ScalaPB <https://github.com/scalapb/ScalaPB>`_. Libraries are created with ``scala_library`` from `rules_scala <https://github.com/bazelbuild/rules_scala>`_

.. list-table:: Rules
   :widths: 1 2
   :header-rows: 1

   * - Rule
     - Description
   * - `scala_proto_compile`_
     - Generates a Scala protobuf ``.jar`` file
   * - `scala_grpc_compile`_
     - Generates Scala protobuf and gRPC ``.jar`` file
   * - `scala_proto_library`_
     - Generates a Scala protobuf library using ``scala_library`` from ``rules_scala``
   * - `scala_grpc_library`_
     - Generates a Scala protobuf and gRPC library using ``scala_library`` from ``rules_scala``

.. _scala_proto_compile:

scala_proto_compile
-------------------

Generates a Scala protobuf ``.jar`` file

Example
*******

Full example project can be found `here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/scala/scala_proto_compile>`__

``WORKSPACE``
^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//scala:repositories.bzl", RULES_PROTO_GRPC_SCALA_MAVEN_ARTIFACTS = "MAVEN_ARTIFACTS", rules_proto_grpc_scala_repos = "scala_repos")
   
   rules_proto_grpc_scala_repos()
   
   load("@io_bazel_rules_scala//:scala_config.bzl", "scala_config")
   
   scala_config()
   
   load("@io_bazel_rules_scala//scala:scala.bzl", "scala_repositories")
   
   scala_repositories()
   
   load("@io_bazel_rules_scala//scala:toolchains.bzl", "scala_register_toolchains")
   
   scala_register_toolchains()
   
   load("@rules_jvm_external//:defs.bzl", "maven_install")
   
   maven_install(
       name = "rules_proto_grpc_scala_maven",
       artifacts = RULES_PROTO_GRPC_SCALA_MAVEN_ARTIFACTS,
       repositories = [
           "https://repo1.maven.org/maven2",
       ],
   )

``BUILD.bazel``
^^^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//scala:defs.bzl", "scala_proto_compile")
   
   scala_proto_compile(
       name = "person_scala_proto",
       protos = ["@rules_proto_grpc//example/proto:person_proto"],
   )
   
   scala_proto_compile(
       name = "place_scala_proto",
       protos = ["@rules_proto_grpc//example/proto:place_proto"],
   )
   
   scala_proto_compile(
       name = "thing_scala_proto",
       protos = ["@rules_proto_grpc//example/proto:thing_proto"],
   )

Attributes
**********

.. list-table:: Attributes for scala_proto_compile
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

- `@rules_proto_grpc//scala:scala_plugin <https://github.com/rules-proto-grpc/rules_proto_grpc/blob/master/scala/BUILD.bazel>`__

.. _scala_grpc_compile:

scala_grpc_compile
------------------

Generates Scala protobuf and gRPC ``.jar`` file

Example
*******

Full example project can be found `here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/scala/scala_grpc_compile>`__

``WORKSPACE``
^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//scala:repositories.bzl", RULES_PROTO_GRPC_SCALA_MAVEN_ARTIFACTS = "MAVEN_ARTIFACTS", rules_proto_grpc_scala_repos = "scala_repos")
   
   rules_proto_grpc_scala_repos()
   
   load("@io_bazel_rules_scala//:scala_config.bzl", "scala_config")
   
   scala_config()
   
   load("@io_bazel_rules_scala//scala:scala.bzl", "scala_repositories")
   
   scala_repositories()
   
   load("@io_bazel_rules_scala//scala:toolchains.bzl", "scala_register_toolchains")
   
   scala_register_toolchains()
   
   load("@io_grpc_grpc_java//:repositories.bzl", "grpc_java_repositories")
   
   grpc_java_repositories()
   
   load("@rules_jvm_external//:defs.bzl", "maven_install")
   
   maven_install(
       name = "rules_proto_grpc_scala_maven",
       artifacts = RULES_PROTO_GRPC_SCALA_MAVEN_ARTIFACTS,
       repositories = [
           "https://repo1.maven.org/maven2",
       ],
   )

``BUILD.bazel``
^^^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//scala:defs.bzl", "scala_grpc_compile")
   
   scala_grpc_compile(
       name = "thing_scala_grpc",
       protos = ["@rules_proto_grpc//example/proto:thing_proto"],
   )
   
   scala_grpc_compile(
       name = "greeter_scala_grpc",
       protos = ["@rules_proto_grpc//example/proto:greeter_grpc"],
   )

Attributes
**********

.. list-table:: Attributes for scala_grpc_compile
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

- `@rules_proto_grpc//scala:grpc_scala_plugin <https://github.com/rules-proto-grpc/rules_proto_grpc/blob/master/scala/BUILD.bazel>`__

.. _scala_proto_library:

scala_proto_library
-------------------

Generates a Scala protobuf library using ``scala_library`` from ``rules_scala``

Example
*******

Full example project can be found `here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/scala/scala_proto_library>`__

``WORKSPACE``
^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//scala:repositories.bzl", RULES_PROTO_GRPC_SCALA_MAVEN_ARTIFACTS = "MAVEN_ARTIFACTS", rules_proto_grpc_scala_repos = "scala_repos")
   
   rules_proto_grpc_scala_repos()
   
   load("@io_bazel_rules_scala//:scala_config.bzl", "scala_config")
   
   scala_config()
   
   load("@io_bazel_rules_scala//scala:scala.bzl", "scala_repositories")
   
   scala_repositories()
   
   load("@io_bazel_rules_scala//scala:toolchains.bzl", "scala_register_toolchains")
   
   scala_register_toolchains()
   
   load("@rules_jvm_external//:defs.bzl", "maven_install")
   
   maven_install(
       name = "rules_proto_grpc_scala_maven",
       artifacts = RULES_PROTO_GRPC_SCALA_MAVEN_ARTIFACTS,
       repositories = [
           "https://repo1.maven.org/maven2",
       ],
   )

``BUILD.bazel``
^^^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//scala:defs.bzl", "scala_proto_library")
   
   scala_proto_library(
       name = "person_scala_proto",
       protos = ["@rules_proto_grpc//example/proto:person_proto"],
       deps = ["place_scala_proto"],
   )
   
   scala_proto_library(
       name = "place_scala_proto",
       protos = ["@rules_proto_grpc//example/proto:place_proto"],
       deps = ["thing_scala_proto"],
   )
   
   scala_proto_library(
       name = "thing_scala_proto",
       protos = ["@rules_proto_grpc//example/proto:thing_proto"],
   )

Attributes
**********

.. list-table:: Attributes for scala_proto_library
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
   * - ``exports``
     - ``label_list``
     - false
     - ``[]``
     - List of labels to pass as exports attr to underlying lang_library rule

.. _scala_grpc_library:

scala_grpc_library
------------------

Generates a Scala protobuf and gRPC library using ``scala_library`` from ``rules_scala``

Example
*******

Full example project can be found `here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/scala/scala_grpc_library>`__

``WORKSPACE``
^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//scala:repositories.bzl", RULES_PROTO_GRPC_SCALA_MAVEN_ARTIFACTS = "MAVEN_ARTIFACTS", rules_proto_grpc_scala_repos = "scala_repos")
   
   rules_proto_grpc_scala_repos()
   
   load("@io_bazel_rules_scala//:scala_config.bzl", "scala_config")
   
   scala_config()
   
   load("@io_bazel_rules_scala//scala:scala.bzl", "scala_repositories")
   
   scala_repositories()
   
   load("@io_bazel_rules_scala//scala:toolchains.bzl", "scala_register_toolchains")
   
   scala_register_toolchains()
   
   load("@io_grpc_grpc_java//:repositories.bzl", "grpc_java_repositories")
   
   grpc_java_repositories()
   
   load("@rules_jvm_external//:defs.bzl", "maven_install")
   
   maven_install(
       name = "rules_proto_grpc_scala_maven",
       artifacts = RULES_PROTO_GRPC_SCALA_MAVEN_ARTIFACTS,
       repositories = [
           "https://repo1.maven.org/maven2",
       ],
   )

``BUILD.bazel``
^^^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//scala:defs.bzl", "scala_grpc_library")
   
   scala_grpc_library(
       name = "thing_scala_grpc",
       protos = ["@rules_proto_grpc//example/proto:thing_proto"],
   )
   
   scala_grpc_library(
       name = "greeter_scala_grpc",
       protos = ["@rules_proto_grpc//example/proto:greeter_grpc"],
       deps = ["thing_scala_grpc"],
   )

Attributes
**********

.. list-table:: Attributes for scala_grpc_library
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
   * - ``exports``
     - ``label_list``
     - false
     - ``[]``
     - List of labels to pass as exports attr to underlying lang_library rule
