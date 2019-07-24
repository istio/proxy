// Copyright 2019 Istio Authors
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

package fakestackdriver

// ServerRequestCountJSON is a JSON string of server request count metric protocol.
const ServerRequestCountJSON = `{  
	"metric":{  
	   "type":"istio.io/service/server/request_count",
	   "labels":{  
		  "destination_owner":"kubernetes://apis/v1/namespaces/server-namespace/pod/server",
		  "destination_port":"20013",
		  "destination_principal":"",
		  "destination_service_name":"localhost:20009",
		  "destination_service_namespace":"server-namespace",
		  "destination_workload_name":"server",
		  "destination_workload_namespace":"server-namespace",
		  "mesh_uid":"",
		  "request_operation":"GET",
		  "request_protocol":"http",
		  "response_code":"200",
		  "service_authentication_policy":"NONE",
		  "source_owner":"kubernetes://apis/v1/namespaces/client-namespace/pod/client",
		  "source_principal":"",
		  "source_workload_name":"client",
		  "source_workload_namespace":"client-namespace"
	   }
	},
	"resource":{  
	   "type":"k8s_container",
	   "labels":{  
		  "cluster_name":"test-cluster",
		  "container_name":"server-container",
		  "location":"test-location",
		  "namespace_name":"server-namespace",
		  "pod_name":"server-pod",
		  "project_id":"test-project"
	   }
	},
	"points":[  
	   {  
		  "value":{  
			 "int64Value":"10"
		  }
	   }
	]
 }`

// ClientRequestCountJSON is a JSON string of client request count metric protocol.
const ClientRequestCountJSON = `{  
	"metric":{  
	   "type":"istio.io/service/client/request_count",
	   "labels":{  
		  "destination_owner":"kubernetes://apis/v1/namespaces/server-namespace/pod/server",
		  "destination_port":"20012",
		  "destination_principal":"",
		  "destination_service_name":"localhost:20009",
		  "destination_service_namespace":"server-namespace",
		  "destination_workload_name":"server",
		  "destination_workload_namespace":"server-namespace",
		  "mesh_uid":"",
		  "request_operation":"GET",
		  "request_protocol":"http",
		  "response_code":"200",
		  "service_authentication_policy":"NONE",
		  "source_owner":"kubernetes://apis/v1/namespaces/client-namespace/pod/client",
		  "source_principal":"",
		  "source_workload_name":"client",
		  "source_workload_namespace":"client-namespace"
	   }
	},
	"resource":{  
	   "type":"k8s_pod",
	   "labels":{  
		  "cluster_name":"test-cluster",
		  "location":"test-location",
		  "namespace_name":"client-namespace",
		  "pod_name":"client-pod",
		  "project_id":"test-project"
	   }
	},
	"points":[  
	   {  
		  "value":{  
			 "int64Value":"10"
		  }
	   }
	]
}`
