:author: rules_proto_grpc
:description: Transitivity changes for rules_proto_grpc Bazel rules
:keywords: Bazel, Protobuf, gRPC, Protocol Buffers, Rules, Build, Starlark, Transitivity, Aspect


Transitivity
============

.. note:: As of release 4.0.0, the transitive mode has been completely removed. All compilation now
   exclusively uses the direct mode.

In previous versions of rules_proto_grpc, the compilation aspect would compile and aggregate all
dependent .proto files from any top level target. In hindsight, this was not the correct behaviour
and led to many bugs, since you may end up creating a library that contains compiled proto files
from a third party, where you should instead be depending on a proper library for that third party's
protos.

Even in a single repo, this may have meant multiple copies of a single compiled proto file being
present in a target, if it is depended on via multiple routes. For some languages, such as C++, this
breaks the 'one definition rule' and produces compilation failures or runtime bugs. For other
languages, such as Python, this just meant unnecessary duplicate files in the output binaries.

Therefore, in the 3.0.0 of rules_proto_grpc, there was added an option to bundle only the
direct proto dependencies into  the libraries, without including the compiled transitive proto
files. This is done by replacing the ``deps`` attr on ``lang_{proto|grpc}_{compile|library}`` with
the ``protos`` attr. Since this would be a substantial breaking change to drop at once on a large
project, the new behaviour was opt-in in 3.0.0 and the old method continued to work throughout the
3.x.x release cycle. As of release 4.0.0, the old transitive mode has been removed completely.

As an additional benefit of this change, we can now support passing arbitrary per-target rules to
protoc through the new ``options`` attr of the rules, which was a much sought after change that was
impossible in the aspect based compilation.

Switching to non-transitive compilation
---------------------------------------

In short, replace ``deps`` with ``protos`` on your targets:

.. code-block:: python

   # Old
   python_grpc_library(
       name = "routeguide",
       deps = ["//example/proto:routeguide_proto"],
   )

   # New
   python_grpc_library(
       name = "routeguide",
       protos = ["//example/proto:routeguide_proto"],
   )

In applying the above change, you may discover that you were inheriting dependencies transitively
and that your builds now fail. In such cases, you should add a
``lang_{proto|grpc}_{compile|library}`` target for those proto files and depend on it explicitly
from the relevant top level binaries/libraries.
