// Copyright 2016 Istio Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// An example implementation of Echo backend in go.

package main

import (
	"flag"
	"fmt"
	"io/ioutil"
	"net/http"
	"strconv"
	"os"
)

var (
	port = flag.Int("port", 8081, "default http port")
	pubkey = ""
	pubkey_path = ""
)

func handler(w http.ResponseWriter, r *http.Request) {
	fmt.Printf("%v %v %v %v\n", r.Method, r.URL, r.Proto, r.RemoteAddr)
	for name, headers := range r.Header {
		for _, h := range headers {
			fmt.Printf("%v: %v\n", name, h)
		}
	}

	fmt.Fprintf(w, "%v", pubkey)
}

func main() {
	pubkey_path = os.Args[1]
	b, err := ioutil.ReadFile(pubkey_path) // just pass the file name
	if err != nil {
		fmt.Print(err)
	}
	//fmt.Println(b) // print the content as 'bytes'
	pubkey = string(b) // convert content to a 'string'
	fmt.Println(pubkey) // print the content as a 'string'


	flag.Parse()

	fmt.Printf("Listening on port %v\n", *port)

	http.HandleFunc("/", handler)
	http.ListenAndServe(":"+strconv.Itoa(*port), nil)
}
