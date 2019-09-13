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

package env

import (
	"fmt"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"time"
)

// Envoy stores data for Envoy process
type Envoy struct {
	cmd    *exec.Cmd
	ports  *Ports
	baseID string
}

// NewClientEnvoy creates a new Client Envoy struct and starts envoy.
func (s *TestSetup) NewClientEnvoy() (*Envoy, error) {
	confTmpl := envoyClientConfTemplYAML
	if s.ClientEnvoyTemplate != "" {
		confTmpl = s.ClientEnvoyTemplate
	}
	baseID := strconv.Itoa(int(s.testName)*2 + 1)

	return newEnvoy(s.ports.ClientAdminPort, confTmpl, baseID, s)
}

// NewServerEnvoy creates a new Server Envoy struct and starts envoy.
func (s *TestSetup) NewServerEnvoy() (*Envoy, error) {
	confTmpl := envoyServerConfTemplYAML
	if s.ServerEnvoyTemplate != "" {
		confTmpl = s.ServerEnvoyTemplate
	}
	baseID := strconv.Itoa(int(s.testName+1) * 2)

	return newEnvoy(s.ports.ServerAdminPort, confTmpl, baseID, s)
}

// Start starts the envoy process
func (s *Envoy) Start(port uint16) error {
	log.Printf("server cmd %v", s.cmd.Args)
	err := s.cmd.Start()
	if err != nil {
		return err
	}

	url := fmt.Sprintf("http://localhost:%v/server_info", port)
	return WaitForHTTPServer(url)
}

// Stop stops the envoy process
func (s *Envoy) Stop(port uint16) error {
	log.Printf("stop envoy ...\n")
	_, _, _ = HTTPPost(fmt.Sprintf("http://127.0.0.1:%v/quitquitquit", port), "", "")
	done := make(chan error, 1)
	go func() {
		done <- s.cmd.Wait()
	}()

	select {
	case <-time.After(3 * time.Second):
		log.Println("envoy killed as timeout reached")
		if err := s.cmd.Process.Kill(); err != nil {
			return err
		}
	case err := <-done:
		log.Printf("stop envoy ... done\n")
		return err
	}

	return nil
}

// TearDown removes shared memory left by Envoy
func (s *Envoy) TearDown() {
	if s.baseID != "" {
		path := "/dev/shm/envoy_shared_memory_" + s.baseID + "0"
		if err := os.Remove(path); err != nil {
			log.Printf("failed to %s\n", err)
		} else {
			log.Printf("removed Envoy's shared memory\n")
		}
	}
}

// NewEnvoy creates a new Envoy struct and starts envoy at the specified port.
func newEnvoy(port uint16, confTmpl, baseID string, s *TestSetup) (*Envoy, error) {
	confPath := filepath.Join(GetDefaultIstioOut(), fmt.Sprintf("config.conf.%v.yaml", port))
	log.Printf("Envoy config: in %v\n", confPath)
	if err := s.CreateEnvoyConf(confPath, confTmpl); err != nil {
		return nil, err
	}

	debugLevel, ok := os.LookupEnv("ENVOY_DEBUG")
	if !ok {
		debugLevel = "info"
	}

	args := []string{"-c", confPath,
		"--drain-time-s", "1",
		"--allow-unknown-fields"}
	if s.stress {
		args = append(args, "--concurrency", "10")
	} else {
		// debug is far too verbose.
		args = append(args, "-l", debugLevel, "--concurrency", "1")
	}
	if s.disableHotRestart {
		args = append(args, "--disable-hot-restart")
	} else {
		args = append(args,
			// base id is shared between restarted envoys
			"--base-id", baseID,
			"--parent-shutdown-time-s", "1",
			"--restart-epoch", strconv.Itoa(s.epoch))
	}
	if s.EnvoyParams != nil {
		args = append(args, s.EnvoyParams...)
	}
	/* #nosec */
	envoyPath := filepath.Join(GetDefaultEnvoyBin(), "envoy")
	if path, exists := os.LookupEnv("ENVOY_PATH"); exists {
		envoyPath = path
	}
	cmd := exec.Command(envoyPath, args...)
	cmd.Stderr = os.Stderr
	cmd.Stdout = os.Stdout
	if s.Dir != "" {
		cmd.Dir = s.Dir
	}
	return &Envoy{
		cmd:    cmd,
		ports:  s.ports,
		baseID: baseID,
	}, nil
}
