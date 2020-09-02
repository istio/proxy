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

package driver

import (
	"fmt"
	"io/ioutil"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"time"

	// Preload proto definitions
	_ "github.com/cncf/udpa/go/udpa/type/v1"
	bootstrap_v3 "github.com/envoyproxy/go-control-plane/envoy/config/bootstrap/v3"
	core "github.com/envoyproxy/go-control-plane/envoy/config/core/v3"

	// Preload proto definitions
	_ "github.com/envoyproxy/go-control-plane/envoy/extensions/filters/network/http_connection_manager/v3"

	"istio.io/proxy/test/envoye2e/env"
)

type Envoy struct {
	// template for the bootstrap
	Bootstrap string

	tmpFile   string
	cmd       *exec.Cmd
	adminPort uint32

	done chan error
}

var _ Step = &Envoy{}

func (e *Envoy) Run(p *Params) error {
	bootstrap, err := p.Fill(e.Bootstrap)
	if err != nil {
		return err
	}
	log.Printf("envoy bootstrap:\n%s\n", bootstrap)

	e.adminPort, err = getAdminPort(bootstrap)
	if err != nil {
		return err
	}
	log.Printf("admin port %d", e.adminPort)

	tmp, err := ioutil.TempFile(os.TempDir(), "envoy-bootstrap-*.yaml")
	if err != nil {
		return err
	}
	if _, err := tmp.Write([]byte(bootstrap)); err != nil {
		return err
	}
	e.tmpFile = tmp.Name()

	debugLevel, ok := os.LookupEnv("ENVOY_DEBUG")
	if !ok {
		debugLevel = "info"
	}
	args := []string{
		"-c", e.tmpFile,
		"-l", debugLevel,
		"--concurrency", "1",
		"--disable-hot-restart",
		"--drain-time-s", "4", // this affects how long draining listenrs are kept alive
	}
	envoyPath := filepath.Join(env.GetDefaultEnvoyBin(), "envoy")
	if path, exists := os.LookupEnv("ENVOY_PATH"); exists {
		envoyPath = path
	}
	cmd := exec.Command(envoyPath, args...)
	cmd.Stderr = os.Stderr
	cmd.Stdout = os.Stdout
	cmd.Dir = BazelWorkspace()

	log.Printf("envoy cmd %v", cmd.Args)
	e.cmd = cmd
	if err = cmd.Start(); err != nil {
		return err
	}
	e.done = make(chan error, 1)
	go func() {
		err := e.cmd.Wait()
		if err != nil {
			log.Printf("envoy process error: %v\n", err)
			if strings.Contains(err.Error(), "segmentation fault") {
				panic(err)
			}
		}
		e.done <- err
	}()

	url := fmt.Sprintf("http://127.0.0.1:%v/ready", e.adminPort)
	return env.WaitForHTTPServer(url)
}

func (e *Envoy) Cleanup() {
	log.Printf("stop envoy ...\n")
	defer os.Remove(e.tmpFile)
	if e.cmd != nil {
		url := fmt.Sprintf("http://127.0.0.1:%v/quitquitquit", e.adminPort)
		_, _, _ = env.HTTPPost(url, "", "")
		select {
		case <-time.After(3 * time.Second):
			log.Println("envoy killed as timeout reached")
			log.Println(e.cmd.Process.Kill())
		case <-e.done:
			log.Printf("stop envoy ... done\n")
		}
	}
}

func getAdminPort(bootstrap string) (uint32, error) {
	pb := &bootstrap_v3.Bootstrap{}
	if err := ReadYAML(bootstrap, pb); err != nil {
		return 0, err
	}
	if pb.Admin == nil || pb.Admin.Address == nil {
		return 0, fmt.Errorf("missing admin section in bootstrap: %v", bootstrap)
	}
	socket, ok := pb.Admin.Address.Address.(*core.Address_SocketAddress)
	if !ok {
		return 0, fmt.Errorf("missing socket in bootstrap: %v", bootstrap)
	}
	port, ok := socket.SocketAddress.PortSpecifier.(*core.SocketAddress_PortValue)
	if !ok {
		return 0, fmt.Errorf("missing port in bootstrap: %v", bootstrap)
	}
	return port.PortValue, nil
}
