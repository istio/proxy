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
	"encoding/json"
	"errors"
	"fmt"
	"io/ioutil"
	"log"
	"net/http"
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

	// istio proxy version to download.
	// This specifies a minor version, and will download the latest dev build of that minor version.
	Version string

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
	} else if _, err := os.Stat(envoyPath); os.IsNotExist(err) && e.Version != "" {
		envoyPath, err = downloadEnvoy(e.Version)
		if err != nil {
			return fmt.Errorf("failed to download Envoy binary %v", err)
		}
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

// downloads env based on the given branch name. Return location of downloaded envoy.
func downloadEnvoy(ver string) (string, error) {
	proxyDepURL := fmt.Sprintf("https://raw.githubusercontent.com/istio/istio/release-%v/istio.deps", ver)
	resp, err := http.Get(proxyDepURL)
	if err != nil {
		return "", fmt.Errorf("cannot get envoy sha from %v: %v", proxyDepURL, err)
	}
	defer resp.Body.Close()
	istioDeps, err := ioutil.ReadAll(resp.Body)
	if err != nil {
		return "", fmt.Errorf("cannot read body of istio deps: %v", err)
	}

	var deps []interface{}
	if err := json.Unmarshal(istioDeps, &deps); err != nil {
		return "", err
	}
	proxySHA := ""
	for _, d := range deps {
		if dm, ok := d.(map[string]interface{}); ok && dm["repoName"].(string) == "proxy" {
			proxySHA = dm["lastStableSHA"].(string)
		}
	}
	if proxySHA == "" {
		return "", errors.New("cannot identify proxy SHA to download")
	}

	// make temp directory to put downloaded envoy binary.
	dir := fmt.Sprintf("%s/%s", os.TempDir(), "istio-proxy")
	if err := os.MkdirAll(dir, 0755); err != nil {
		return "", err
	}
	dst := fmt.Sprintf("%v/envoy-%v", dir, proxySHA)
	if _, err := os.Stat(dst); err == nil {
		return dst, nil
	}
	envoyURL := fmt.Sprintf("https://storage.googleapis.com/istio-build/proxy/envoy-alpha-%v.tar.gz", proxySHA)
	downloadCmd := exec.Command("bash", "-c", fmt.Sprintf("curl -fLSs %v | tar xz", envoyURL))
	downloadCmd.Stderr = os.Stderr
	downloadCmd.Stdout = os.Stdout
	err = downloadCmd.Run()
	if err != nil {
		return "", fmt.Errorf("fail to run envoy download command: %v", err)
	}
	src := "usr/local/bin/envoy"
	if _, err := os.Stat(src); err != nil {
		return "", fmt.Errorf("fail to find downloaded envoy: %v", err)
	}
	defer os.RemoveAll("usr/")

	cpCmd := exec.Command("cp", src, dst)
	cpCmd.Stderr = os.Stderr
	cpCmd.Stdout = os.Stdout
	if err := cpCmd.Run(); err != nil {
		return "", fmt.Errorf("fail to copy envoy binary from %v to %v: %v", src, dst, err)
	}

	return dst, nil
}
