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

package main

import (
	"log"

	fs "istio.io/proxy/test/envoye2e/stackdriver_plugin/fake_stackdriver"
)

func main() {
	log.Println("Run Stackdriver server, listening on port 80")
	if err := fs.Run(80); err != nil {
		log.Printf("Stackdriver server failed %v", err)
	}
}
