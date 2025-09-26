:author: rules_proto_grpc
:description: rules_proto_grpc Bazel rules for Ruby
:keywords: Bazel, Protobuf, gRPC, Protocol Buffers, Rules, Build, Starlark, Ruby


Ruby
====

Rules for generating Ruby protobuf and gRPC ``.rb`` files and libraries using standard Protocol Buffers and gRPC. Libraries are created with ``ruby_library`` from `rules_ruby <https://github.com/bazelruby/rules_ruby>`_

.. list-table:: Rules
   :widths: 1 2
   :header-rows: 1

   * - Rule
     - Description
   * - `ruby_proto_compile`_
     - Generates Ruby protobuf ``.rb`` files
   * - `ruby_grpc_compile`_
     - Generates Ruby protobuf and gRPC ``.rb`` files
   * - `ruby_proto_library`_
     - Generates a Ruby protobuf library using ``ruby_library`` from ``rules_ruby``
   * - `ruby_grpc_library`_
     - Generates a Ruby protobuf and gRPC library using ``ruby_library`` from ``rules_ruby``

.. _ruby_proto_compile:

ruby_proto_compile
------------------

Generates Ruby protobuf ``.rb`` files

Example
*******

Full example project can be found `here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/ruby/ruby_proto_compile>`__

``WORKSPACE``
^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//ruby:repositories.bzl", rules_proto_grpc_ruby_repos = "ruby_repos")
   
   rules_proto_grpc_ruby_repos()
   
   load("@bazelruby_rules_ruby//ruby:deps.bzl", "rules_ruby_dependencies", "rules_ruby_select_sdk")
   
   rules_ruby_dependencies()
   
   rules_ruby_select_sdk(version = "3.1.1")
   
   load("@bazelruby_rules_ruby//ruby:defs.bzl", "ruby_bundle")
   
   ruby_bundle(
       name = "rules_proto_grpc_bundle",
       gemfile = "@rules_proto_grpc//ruby:Gemfile",
       gemfile_lock = "@rules_proto_grpc//ruby:Gemfile.lock",
       includes = {"grpc": ["etc"]},
   )

``BUILD.bazel``
^^^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//ruby:defs.bzl", "ruby_proto_compile")
   
   ruby_proto_compile(
       name = "person_ruby_proto",
       protos = ["@rules_proto_grpc//example/proto:person_proto"],
   )
   
   ruby_proto_compile(
       name = "place_ruby_proto",
       protos = ["@rules_proto_grpc//example/proto:place_proto"],
   )
   
   ruby_proto_compile(
       name = "thing_ruby_proto",
       protos = ["@rules_proto_grpc//example/proto:thing_proto"],
   )

Attributes
**********

.. list-table:: Attributes for ruby_proto_compile
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

- `@rules_proto_grpc//ruby:ruby_plugin <https://github.com/rules-proto-grpc/rules_proto_grpc/blob/master/ruby/BUILD.bazel>`__

.. _ruby_grpc_compile:

ruby_grpc_compile
-----------------

Generates Ruby protobuf and gRPC ``.rb`` files

Example
*******

Full example project can be found `here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/ruby/ruby_grpc_compile>`__

``WORKSPACE``
^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//ruby:repositories.bzl", rules_proto_grpc_ruby_repos = "ruby_repos")
   
   rules_proto_grpc_ruby_repos()
   
   load("@bazelruby_rules_ruby//ruby:deps.bzl", "rules_ruby_dependencies", "rules_ruby_select_sdk")
   
   rules_ruby_dependencies()
   
   rules_ruby_select_sdk(version = "3.1.1")
   
   load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")
   
   grpc_deps()
   
   load("@bazelruby_rules_ruby//ruby:defs.bzl", "ruby_bundle")
   
   ruby_bundle(
       name = "rules_proto_grpc_bundle",
       gemfile = "@rules_proto_grpc//ruby:Gemfile",
       gemfile_lock = "@rules_proto_grpc//ruby:Gemfile.lock",
       includes = {"grpc": ["etc"]},
   )

``BUILD.bazel``
^^^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//ruby:defs.bzl", "ruby_grpc_compile")
   
   ruby_grpc_compile(
       name = "thing_ruby_grpc",
       protos = ["@rules_proto_grpc//example/proto:thing_proto"],
   )
   
   ruby_grpc_compile(
       name = "greeter_ruby_grpc",
       protos = ["@rules_proto_grpc//example/proto:greeter_grpc"],
   )

Attributes
**********

.. list-table:: Attributes for ruby_grpc_compile
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

- `@rules_proto_grpc//ruby:ruby_plugin <https://github.com/rules-proto-grpc/rules_proto_grpc/blob/master/ruby/BUILD.bazel>`__
- `@rules_proto_grpc//ruby:grpc_ruby_plugin <https://github.com/rules-proto-grpc/rules_proto_grpc/blob/master/ruby/BUILD.bazel>`__

.. _ruby_proto_library:

ruby_proto_library
------------------

Generates a Ruby protobuf library using ``ruby_library`` from ``rules_ruby``

Example
*******

Full example project can be found `here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/ruby/ruby_proto_library>`__

``WORKSPACE``
^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//ruby:repositories.bzl", rules_proto_grpc_ruby_repos = "ruby_repos")
   
   rules_proto_grpc_ruby_repos()
   
   load("@bazelruby_rules_ruby//ruby:deps.bzl", "rules_ruby_dependencies", "rules_ruby_select_sdk")
   
   rules_ruby_dependencies()
   
   rules_ruby_select_sdk(version = "3.1.1")
   
   load("@bazelruby_rules_ruby//ruby:defs.bzl", "ruby_bundle")
   
   ruby_bundle(
       name = "rules_proto_grpc_bundle",
       gemfile = "@rules_proto_grpc//ruby:Gemfile",
       gemfile_lock = "@rules_proto_grpc//ruby:Gemfile.lock",
       includes = {"grpc": ["etc"]},
   )

``BUILD.bazel``
^^^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//ruby:defs.bzl", "ruby_proto_library")
   
   ruby_proto_library(
       name = "person_ruby_proto",
       protos = ["@rules_proto_grpc//example/proto:person_proto"],
       deps = ["place_ruby_proto"],
   )
   
   ruby_proto_library(
       name = "place_ruby_proto",
       protos = ["@rules_proto_grpc//example/proto:place_proto"],
       deps = ["thing_ruby_proto"],
   )
   
   ruby_proto_library(
       name = "thing_ruby_proto",
       protos = ["@rules_proto_grpc//example/proto:thing_proto"],
   )

Attributes
**********

.. list-table:: Attributes for ruby_proto_library
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

.. _ruby_grpc_library:

ruby_grpc_library
-----------------

Generates a Ruby protobuf and gRPC library using ``ruby_library`` from ``rules_ruby``

Example
*******

Full example project can be found `here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/ruby/ruby_grpc_library>`__

``WORKSPACE``
^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//ruby:repositories.bzl", rules_proto_grpc_ruby_repos = "ruby_repos")
   
   rules_proto_grpc_ruby_repos()
   
   load("@bazelruby_rules_ruby//ruby:deps.bzl", "rules_ruby_dependencies", "rules_ruby_select_sdk")
   
   rules_ruby_dependencies()
   
   rules_ruby_select_sdk(version = "3.1.1")
   
   load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")
   
   grpc_deps()
   
   load("@bazelruby_rules_ruby//ruby:defs.bzl", "ruby_bundle")
   
   ruby_bundle(
       name = "rules_proto_grpc_bundle",
       gemfile = "@rules_proto_grpc//ruby:Gemfile",
       gemfile_lock = "@rules_proto_grpc//ruby:Gemfile.lock",
       includes = {"grpc": ["etc"]},
   )

``BUILD.bazel``
^^^^^^^^^^^^^^^

.. code-block:: python

   load("@rules_proto_grpc//ruby:defs.bzl", "ruby_grpc_library")
   
   ruby_grpc_library(
       name = "thing_ruby_grpc",
       protos = ["@rules_proto_grpc//example/proto:thing_proto"],
   )
   
   ruby_grpc_library(
       name = "greeter_ruby_grpc",
       protos = ["@rules_proto_grpc//example/proto:greeter_grpc"],
       deps = ["thing_ruby_grpc"],
   )

Attributes
**********

.. list-table:: Attributes for ruby_grpc_library
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
