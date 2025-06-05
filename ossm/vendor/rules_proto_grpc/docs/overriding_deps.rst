:author: rules_proto_grpc
:description: Overriding dependencies
:keywords: Bazel, Protobuf, gRPC, Protocol Buffers, Rules, Build, Workspace, Dependencies

.. _sec_overriding_deps:

Overriding Dependencies
=======================

These rules ship with a set of working and tested dependencies that are generally up-to-date at the
time a release is made. However, since rule updates typically occur less frequently than updates in
the entire tree of dependencies, it is often useful to override the versions of specific
dependencies in a local workspace.

To do this, the newer version must be specified in your WORKSPACE file **before** the
rules_proto_grpc dependencies are loaded. This works because within a Bazel workspace, the standard
behaviour is the that first declaration of a specific name 'wins' and later declarations are usually
skipped.

For example, to use a specific Protobuf version:

.. code-block:: python

   http_archive(
     name = "com_google_protobuf",
     sha256 = "8b28fdd45bab62d15db232ec404248901842e5340299a57765e48abe8a80d930",
     strip_prefix = "protobuf-3.20.1",
     urls = ["https://github.com/protocolbuffers/protobuf/archive/v3.20.1.tar.gz"],
   )

   # rules_proto_grpc load MUST be below here

Similarly, for gRPC:

.. code-block:: python

   http_archive(
     name = "com_github_grpc_grpc",
     sha256 = "8c05641b9f91cbc92f51cc4a5b3a226788d7a63f20af4ca7aaca50d92cc94a0d",
     strip_prefix = "grpc-1.44.0",
     urls = ["https://github.com/grpc/grpc/archive/v1.44.0.tar.gz"],
   )

   # rules_proto_grpc load MUST be below here
