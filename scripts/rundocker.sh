#!/bin/bash
docker run -it -v $(pwd)/proxy:/work -v $(pwd)/envoy:/envoy gcr.io/istio-testing/build-tools-proxy:master-2021-06-07T22-22-51 -- bash
