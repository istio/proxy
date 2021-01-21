// Copyright Istio Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package extensionserver

import (
	"log"

	core "github.com/envoyproxy/go-control-plane/envoy/config/core/v3"
	wasm "github.com/envoyproxy/go-control-plane/envoy/extensions/filters/http/wasm/v3"
	v3 "github.com/envoyproxy/go-control-plane/envoy/extensions/wasm/v3"
	ptypes "github.com/golang/protobuf/ptypes"
	"github.com/golang/protobuf/ptypes/wrappers"
)

// Convert to an envoy config.
// It so happens that 1.7 and 1.8 match in terms protobuf bytes, but not JSON.
func Convert(ext *Extension) (*core.TypedExtensionConfig, error) {
	// wrap configuration into StringValue
	json, err := ext.Configuration.MarshalJSON()
	if err != nil {
		return nil, err
	}
	configuration, err := ptypes.MarshalAny(&wrappers.StringValue{Value: string(json)})
	if err != nil {
		return nil, err
	}

	// detect the runtime
	runtime := "envoy.wasm.runtime.v8"
	bytes := prefetch[ext.SHA256]
	switch ext.Runtime {
	case "v8", "":
		break
	case "wavm":
		runtime = "envoy.wasm.runtime.wavm"
	case "null":
		runtime = "envoy.wasm.runtime.null"
		bytes = []byte(ext.Path)
	default:
		log.Printf("unknown runtime %q, defaulting to v8\n", ext.Runtime)
	}

	// create plugin config
	plugin := &wasm.Wasm{
		Config: &v3.PluginConfig{
			RootId: ext.RootID,
			Vm: &v3.PluginConfig_VmConfig{
				VmConfig: &v3.VmConfig{
					VmId:    ext.VMID,
					Runtime: runtime,
					Code: &core.AsyncDataSource{
						Specifier: &core.AsyncDataSource_Local{
							Local: &core.DataSource{
								Specifier: &core.DataSource_InlineBytes{
									InlineBytes: bytes,
								},
							},
						},
					},
					AllowPrecompiled: true,
				},
			},
			Configuration: configuration,
		},
	}

	typed, err := ptypes.MarshalAny(plugin)
	if err != nil {
		return nil, err
	}
	return &core.TypedExtensionConfig{
		Name:        ext.Name,
		TypedConfig: typed,
	}, nil
}
