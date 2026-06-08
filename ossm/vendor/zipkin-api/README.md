# Zipkin API

[![Gitter chat](http://img.shields.io/badge/gitter-join%20chat%20%E2%86%92-brightgreen.svg)](https://gitter.im/openzipkin/zipkin)
[![Build Status](https://github.com/openzipkin/zipkin-api/workflows/test/badge.svg)](https://github.com/openzipkin/zipkin-api/actions?query=workflow%3Atest)
[![Maven Central](https://img.shields.io/maven-central/v/io.zipkin.proto3/zipkin-proto3.svg)](https://search.maven.org/search?q=g:io.zipkin.proto3%20AND%20a:zipkin-proto3)

Zipkin API includes service and model definitions used for
Zipkin-compatible services.

This repository includes [OpenAPI Spec](./zipkin2-api.yaml) as well
[Protocol Buffers](./zipkin.proto) and [Thrift](thrift) interchange formats. As these
IDL files are languagage agnostic, there are no compilation instructions needed or included.

## Language independent interchange format for Zipkin transports
* [Protocol Buffers v3](./zipkin.proto) - Requires Zipkin 2.8+ or similar to parse it.
* [Thrift](./thrift) - Deprecated as new clients should not generate this format

## OpenApi (Http endpoint of the zipkin server)
* [/api/v1](./zipkin-api.yaml) - Still supported on zipkin-server
* [/api/v2](./zipkin2-api.yaml) - Most recent and published [here](https://zipkin.io/zipkin-api/#/)

Take a look at the [example repository](https://github.com/openzipkin/zipkin-api-example) for how to use this.

## Artifacts
The proto artifact published is `zipkin-proto3` under the group ID `io.zipkin.proto3`

### Library Releases
Releases are at [Sonatype](https://oss.sonatype.org/content/repositories/releases) and [Maven Central](http://search.maven.org/#search%7Cga%7C1%7Cg%3A%22io.zipkin.proto3%22)

### Library Snapshots
Snapshots are uploaded to [Sonatype](https://oss.sonatype.org/content/repositories/snapshots) after
commits to master.
