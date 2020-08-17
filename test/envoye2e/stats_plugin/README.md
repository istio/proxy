# Integration Test for WASM Filter
---
The following will provide an overview on how to add an Integration Test for [AttributeGen WASM Filter](https://istio.io/latest/docs/reference/config/proxy_extensions/attributegen/).

First of all it's important to know some key aspects about the filter that we are testing such as:
  - What's the **function** of the filter? (In this case it can be found in istio.io)
  - Does the filter **interact** with another filter in order to work? (For example Stats Filter to produce metrics)
  - What **configuration** is needed for the plugin to work? Do we need additional configuration?

### AttributeGen Configuration 
---

The configuration for AttributeGen Filter should be templified and can be divided into three sections. This configuration can be found under *proxy/testdata/filters/attributegen.yaml.tmpl*

The first section contains information about the filter such as the name of the plugin and typed configuration in order to add it to the Envoy Filter. 
```js
- name: istio.attributegen
  typed_config:
    "@type": type.googleapis.com/udpa.type.v1.TypedStruct
    type_url: type.googleapis.com/envoy.extensions.filters.http.wasm.v3.Wasm
```
 The second section contains configuration regarding the vm_id, runtime (null or v8) and the plugin's code (wasm module or reserved keyword). It's important to note the fact that the values of these fields are enclosed in **{{ }}**. These are called template directives that have built-in objects inside them. More information about this can be found in [Helm docs](https://helm.sh/docs/chart_template_guide/getting_started/). The runtime and local values are referenced from the Go code (stats_test.go file) which will make more sense later on. 
```js
    value:
          config:
            vm_config:
              vm_id: attributegen{{ .N }}
              runtime: {{ .Vars.AttributeGenWasmRuntime }}
              code:
                local: { {{ .Vars.AttributeGenFilterConfig }} }
```
Finally, we have the JSON string configuration to test the filter. Information regarding the output_attribute and match fields can be found by looking at the filter's [docs](https://istio.io/latest/docs/reference/config/proxy_extensions/attributegen/). In this case, the AttributeGen filter classifies response codes (2xx instead of 200). The value '2xx' will be assigned to `istio_responseClass` attribute when the condition is met which is mapped and reference in the Stats filter. 
```js
            configuration:
                      "@type": "type.googleapis.com/google.protobuf.StringValue"
                      value: |
                        {
                          "attributes": [
                            {
                              "output_attribute": "istio_responseClass",
                              "match": [
                                {
                                  "value": "2xx",
                                  "condition": "response.code >= 200 && response.code <= 299"
                                }
                              ]
                            }
                          ]
                        }
```

### Stats Filter Configuration
---

After reading the AttributeGen docs, we find that the Stats plugin produces metrics of the AttributeGen filter. Now we need to populate a YAML configuration file (*proxy/testdata/stats/stats_filter_config.yaml*) to map `response_code` dimension in the `requests_total` [metric](https://istio.io/latest/docs/reference/config/policy-and-telemetry/metrics/) to `istio_responseClass` attribute as seen below. These fields are part of the [Stats PluginConfig](https://istio.io/latest/docs/reference/config/proxy_extensions/stats/) documentation.
```js
debug: "true"
max_peer_cache_size: 20
stat_prefix: istio
field_separator: ";.;"
metrics:
  - name: requests_total
    dimensions:
      response_code: istio_responseClass
```

### Integration Test (Go)
---

The integration test code logic happens in: *proxy/test/envoye2e/stats_plugin/stats_test.go*
At a high level it can be observed that the test has four sections:

#### 1. Setting of parameters such as:

  - Plugin code configuration for MetadaExchange, Stats and AttributeGen filter. 
  - WasmRuntime for AttributeGen filter and another WasmRuntime for MetadataExchange and Stats Filter (envoy.wasm.runtime.null). 
  - Finally, we have StatsConfig related to Envoy boostrap configuration, Client and Server configuration for the Stats Filter. We can see that stats_filter_config.yaml configuration file is associated with StatsFilterServerConfig because we'll generate metrics on the server side. 
  - It's worth to note that the values for AttributeGenFilterConfig and AttributeGenWasmRuntime are taken from the AttributeGenRuntime struct. 
```js
var AttributeGenRuntimes = []struct {
	AttributeGenFilterCode string
	WasmRuntime     string
}{
	{
		AttributeGenFilterCode:    "inline_string: \"envoy.wasm.attributegen\"",
		WasmRuntime:        	   "envoy.wasm.runtime.null",
	},
	{
		AttributeGenFilterCode:    "filename: " + filepath.Join(env.GetBazelOptOut(), "extensions/attributegen.wasm"),
		WasmRuntime:        	   "envoy.wasm.runtime.v8",
	},
}
```
```js
params := driver.NewTestParams(t, map[string]string{
				"RequestCount":               "10",
				"MetadataExchangeFilterCode": "inline_string: \"envoy.wasm.metadata_exchange\"",
				"StatsFilterCode": 			  "inline_string: \"envoy.wasm.stats\"",
				"AttributeGenFilterConfig":   runtime.AttributeGenFilterCode,
				"AttributeGenWasmRuntime":    runtime.WasmRuntime,
				"WasmRuntime":				  "envoy.wasm.runtime.null",
				"StatsConfig":                driver.LoadTestData("testdata/bootstrap/stats.yaml.tmpl"),
				"StatsFilterClientConfig":    driver.LoadTestJSON("testdata/stats/client_config.yaml"),
				"StatsFilterServerConfig":    driver.LoadTestJSON("testdata/stats/request_classification_config.yaml"),
				"ResponseCodeClass":		  "2xx",
			}, envoye2e.ProxyE2ETests)
```
Now that we know how to set the parameters inside the test, we can go back to this section from the AttribteGen plugin configuration:
```js
    value:
          config:
            vm_config:
              vm_id: attributegen{{ .N }}
              runtime: {{ .Vars.AttributeGenWasmRuntime }}
              code:
                local: { {{ .Vars.AttributeGenFilterConfig }} }
```
The runtime and local values are taken from the Params struct. The first test case will assigne a `envoy.wasm.runtime.null` to runtime and `"inline_string: \"envoy.wasm.attributegen\""` to local. During the second test case runtime takes the value of `envoy.wasm.runtime.v8` and local will take the filter's WASM module that was generated.
####  2. Client and server metadata configuration.

```js
params.Vars["ClientMetadata"] = params.LoadTestData("testdata/client_node_metadata.json.tmpl")
params.Vars["ServerMetadata"] = params.LoadTestData("testdata/server_node_metadata.json.tmpl")
```
####  3. HTTP Filter configuration. 

This section is important because the configuration for AttributeGen filter is loaded here into the ServerHTTPFilter together with the StatsInbound file that retrieves the Stats configuration data from the StatsServerConfig parameter mentioned before.
```js
 params.Vars["ServerHTTPFilters"] = params.LoadTestData("testdata/filters/attributegen.yaml.tmpl") + "\n" + params.LoadTestData("testdata/filters/stats_inbound.yaml.tmpl")
 params.Vars["ClientHTTPFilters"] = params.LoadTestData("testdata/filters/stats_outbound.yaml.tmpl")
```

#### 4. Scenario with a series of Steps. 

It's worth to mention that the first 3 steps are common among other integration tests.
  - XDS server is initialized. 
  - An update to the client and server listener is done. 
  - Setting the Envoy bootstrap configuration. 
  - Generating traffic 10 times (from client to server in this case).
  - Producing the server-side metrics (`istio_requests_total`). 
```js
if err := (&driver.Scenario{
      []driver.Step{
        &driver.XDS{},
        &driver.Update{Node: "client", Version: "0", Listeners:[]string{params.LoadTestData("testdata/listener/client.yaml.tmpl")}},
        &driver.Update{Node: "server", Version: "0", Listeners:[]string{params.LoadTestData("testdata/listener/server.yaml.tmpl")}},
        &driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/server.yaml.tmpl")},
        &driver.Envoy{Bootstrap: params.LoadTestData("testdata/bootstrap/client.yaml.tmpl")},
        &driver.Sleep{Duration: 1 * time.Second},
        &driver.Repeat{N: 10,
          Step: &driver.HTTPCall{
            Port: params.Ports.ClientPort,
            Body: "hello, world!",
          },
        },
        &driver.Stats{params.Ports.ServerAdmin, map[string]driver.StatMatcher{
          "istio_requests_total": &driver.ExactStat{"testdata/metric/server_request_total.yaml.tmpl"},
        }},
      },
    }).Run(params); err != nil {
      t.Fatal(err)
```

### Running the test
---

To be able to run the test you will need to add the test name into *proxy/test/envoye2e/inventory.go*
The test was run under the following directory and go command:
```sh
/proxy/test/envoye2e/stats_plugin$ go test -v -run TestAttributeGen
```

