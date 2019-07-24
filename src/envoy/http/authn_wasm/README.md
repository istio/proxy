Istio Authentication Filter in WASM
-----------------------------------

This is an experimental implementation of Istio Authentication filter in WASM. The objective is to
evaluate the functionality, testing, debugging and anything related in the integration.

To test the `authn_wams` filter:
1. Start the proxy binary with the `envoy.yaml`:
    ```bash
    proxy -c envoy.yaml -l debug
    ```

1. Send request to port 8090:
    ```bash
    curl -s http://127.0.0.1:8090
    ```

1. Check the logging for `Applied authentication filter config:` to confirm the request is
   processed by the `authn_wasm` filter.
