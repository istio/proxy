# Extension server tool

This is a tool to apply configuration using ExtensionConfigDiscovery mechanism in Envoy.
The tool allows rapid pushes of Wasm modules and Wasm configuration. It accepts two flags:

- `-p` for the port to listen on.
- `-c` for the directory to watch configuration files for.

An example configuration is provided [here](/tools/extensionserver/testdata/stats.yaml).
Each extension configuration has the following fields:

- `name` is the name of the resource in ECDS.
- `path` or `url` for the Wasm code.
- `sha256` for the Wasm code SHA256 hash. The code is not refetched if the hash
  matches. The config is rejected if SHA256 is specified but does not match.
- `vm_id` and `root_id` optional settings for the VM and root context.
- `configuration` JSON configuration to apply for the extension.

## Usage

An experimental build of this server is available as a docker image gcr.io/istio-testing/extensionserver:alpha0.

0. Follow instructions to install Istio 1.7 release, and install a demo application `bookinfo`.

0. Deploy the [extension server](extensionserver.yaml) in "default" namespace.:
  
    kubectl apply -f extensionserver.yaml

0. Note the address of the extension server in Istio parlor: `outbound|8080|grpc|extensionserver.default.svc.cluster.local`. You may need to adjust the address if using a different namespace. It is recommended that the sidecar is injected into the extensionserver.

0. Validate that stats continue to be incremented:

    kubectl exec -it productpage-v1-64794f5db4-zr7cr -c istio-proxy -- curl localhost:15000/stats/prometheus | grep istio_requests_total | grep reporter=\"destination\"

