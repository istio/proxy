As part of the ongoing Telemetry V2 effort to move Mixer like processing to the proxy as Envoy filters, we are releasing experimental support for Stackdriver HTTP telemetry. 

It is a replacement for the current Mixer Stackdriver adapter, which supports exporting [GCP Istio standard metric](https://cloud.google.com/monitoring/api/metrics_istio), server access log, and traces from the proxy.

## How to enable

### Metrics and Access Logs

Metrics and server side access log will be enabled by default by installing the Stackdriver filter.

1. Disable `Stackdriver adapter` or `istio-telemetry`

   To avoid duplicated telemetry reporting, you should disable `Stackdriver adapter` if you already set it up with Mixer v1. To disable Stackdriver adapter, remove Stackdriver Mixer rules, handlers, and instances, e.g. run `kubectl delete -n istio-system rule your-stackdriver-rule && kubectl delete -n istio-system handler your-stackdriver-handlers && kubectl delete -n istio-system instance your-stackdriver-instance`

   If you want to disable istio-telemetry as whole:
   * If your cluster is installed using `istioctl`, run `istioctl manifest apply --set values.mixer.telemetry.enabled=false,values.mixer.policy.enabled=false`. If your cluster is installed via helm, run `helm template install/kubernetes/helm/istio --name istio --namespace istio-system --set mixer.telemetry.enabled=false --set mixer.policy.enabled=false`.
   * Alternatively, you can comment out mixerCheckServer and mixerReportServer in your mesh configuration.

2. Enable metadata exchange filter
      `kubectl -n istio-system apply -f https://raw.githubusercontent.com/istio/proxy/release-1.4/extensions/stats/testdata/istio/metadata-exchange_filter.yaml`

3. Enable Stackdriver filter
   `kubectl -n istio-system apply -f https://raw.githubusercontent.com/istio/proxy/release-1.4/extensions/stackdriver/testdata/stackdriver_filter.yaml`

4. Visit Stackdriver Monitoring metric explorer and search for [standard Istio metrics](https://cloud.google.com/monitoring/api/metrics_istio). Visit Stackdriver Logging Viewer and search `server-accesslog-stackdriver` for access log entries.

### Trace

Opencensus tracer is by default shipped with 1.4.0 Istio proxy, which supports exporting traces to Stackdriver as other tracers liker Jeager and Zipkin. To enable it, set global tracer as Stackdriver: `helm template --set global.proxy.tracer="stackdriver" install/kubernetes/helm/istio --name istio --namespace istio-system | kubectl apply -f -`. The default sampling rate is 1%. To raise it, you could set it via traceSampling helm option: `--set pilot.traceSampling=100`

## Limitations in 1.4.0
1. No TCP telemetry.
2. Access log misses some labels, which will be added in the following 1.4.x releases.

## Details
1. The preview version uses the WASM sandbox API, but *does not* run inside a WASM VM. It is natively compiled in Envoy using `NullVM`.
2. In later release we will enable running filters in the V8 WASM VM.
3. At present The filters are configured using the Istio Envoy Filter API.
