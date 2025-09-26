:author: rules_proto_grpc
:description: Example usage for the rules_proto_grpc Bazel rules
:keywords: Bazel, Protobuf, gRPC, Protocol Buffers, Rules, Build, Starlark, Example, Usage


Example Usage
=============

These steps walk through the actions required to go from a raw ``.proto`` file to a C++ library.
Other languages will have a similar high-level layout, but you should check the language specific
pages too.

The full example workspaces for C++ can be found
`here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/cpp>`__, along with
the demo proto files
`here <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example/proto>`__. Example
workspaces for all other languages can also be found in
`that same directory <https://github.com/rules-proto-grpc/rules_proto_grpc/tree/master/example>`__.


**Step 1**: Load rules_proto_grpc
------------------------------------

First, follow :ref:`the instructions <sec_installation>` to load the rules_proto_grpc rules in your
``WORKSPACE`` file.


**Step 2**: Write a ``.proto`` file
-----------------------------------

Write a protobuf ``.proto file``, following
`the specification <https://developers.google.com/protocol-buffers/docs/proto3>`__. In this case,
we've called the file ``thing.proto``.

.. code-block:: proto

   syntax = "proto3";

   package example;

   import "google/protobuf/any.proto";

   message Thing {
       string name = 1;
       google.protobuf.Any payload = 2;
   }


**Step 3**: Write a ``BUILD`` file
----------------------------------

This file should introduce a
`proto_library <https://docs.bazel.build/versions/master/be/protocol-buffer.html#proto_library>`_
target:

.. code-block:: python

   proto_library(
       name = "thing_proto",
       srcs = ["thing.proto"],
       deps = ["@com_google_protobuf//:any_proto"],
   )

This rule takes no visible action, but is used to group a set of related ``.proto`` files and their
dependencies. In this example we have a dependency on a well-known type ``any.proto``, hence the
``proto_library`` to ``proto_library`` dependency (``"@com_google_protobuf//:any_proto"``)


**Step 4**: Add a ``cpp_proto_compile`` target
----------------------------------------------

We now add a target using the ``cpp_proto_compile`` rule. This rule converts our ``.proto`` file
into the C++ specific ``.h`` and ``.cc`` files.

.. note:: In this example ``thing.proto`` does not include service definitions (gRPC). For protos
   with services, use the ``cpp_grpc_compile`` rule instead.

.. code-block:: python

   # BUILD.bazel
   load("@rules_proto_grpc//cpp:defs.bzl", "cpp_proto_compile")

   cpp_proto_compile(
       name = "cpp_thing_proto",
       protos = [":thing_proto"],
   )


**Step 5**: Load the ``WORKSPACE`` macro
----------------------------------------

But wait, before we can build this, we need to load the dependencies necessary for this rule in our
``WORKSPACE`` (see :ref:`cpp_proto_compile`):

.. code-block:: python

   # WORKSPACE
   load("@rules_proto_grpc//cpp:repositories.bzl", "cpp_repos")

   cpp_repos()


**Step 6**: Build it!
---------------------

We can now build the ``cpp_thing_proto`` target:

.. code-block:: bash

   $ bazel build //example/proto:cpp_thing_proto
   Target //example/proto:cpp_thing_proto up-to-date:
     bazel-genfiles/example/proto/cpp_thing_proto/example/proto/thing.pb.h
     bazel-genfiles/example/proto/cpp_thing_proto/example/proto/thing.pb.cc

You should now see generated ``.cc`` and ``.h`` files in your bazel-bin output tree.


**Step 7**: Create a library
----------------------------

If we were only interested in the generated files, the ``cpp_grpc_compile`` rule would be fine.
However, for convenience we'd rather have the outputs compiled into a C++ library with the necessary
dependencies linked. To do that, let's change the  rule from ``cpp_proto_compile`` to
``cpp_proto_library``:

.. code-block:: python

   # BUILD.bazel
   load("@rules_proto_grpc//cpp:defs.bzl", "cpp_proto_library")

   cpp_proto_library(
       name = "cpp_thing_proto",
       protos = [":thing_proto"],
   )

Now we can build again:

.. code-block:: bash

   $ bazel build //example/proto:cpp_thing_proto
   Target //example/proto:cpp_thing_proto up-to-date:
     bazel-bin/example/proto/libcpp_thing_proto.a
     bazel-bin/example/proto/libcpp_thing_proto.so
     bazel-genfiles/example/proto/cpp_thing_proto/example/proto/thing.pb.h
     bazel-genfiles/example/proto/cpp_thing_proto/example/proto/thing.pb.cc

This time, we also have ``.a`` and ``.so`` files built. We can now use
``//example/proto:cpp_thing_proto`` as a dependency of any other ``cc_library`` or ``cc_binary``
target as per normal.

.. note:: The ``cpp_proto_library`` target implicitly calls ``cpp_proto_compile``, and we can access
   that rule's by adding ``_pb`` at the end of the target name, like
   ``bazel build //example/proto:cpp_thing_proto_pb``
