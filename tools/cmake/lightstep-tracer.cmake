# tracer
include_directories(
        vendor/lightstep/src/c++11
        genfiles/src/lightstep/lightstep-tracer-common
)

set(TRACER_SOURCE
        genfiles/collector.pb.cc
        genfiles/lightstep_carrier.pb.cc

        vendor/lightstep/src/c++11/impl.cc
        vendor/lightstep/src/c++11/span.cc
        vendor/lightstep/src/c++11/tracer.cc
        vendor/lightstep/src/c++11/util.cc
        )


add_library(tracer STATIC ${TRACER_SOURCE})
target_compile_definitions(tracer PRIVATE -DPACKAGE_VERSION="2")
