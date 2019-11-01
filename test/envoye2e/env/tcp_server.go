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
	"bufio"
	"crypto/tls"
	"crypto/x509"
	"fmt"
	"io"
	"io/ioutil"
	"log"
	"net"
	"path/filepath"
	"time"
)

// TCPServer stores data for a TCP server.
type TCPServer struct {
	port      uint16
	lis       net.Listener
	prefix    string
	enableTLS bool
	dir       string
}

// NewTCPServer creates a new TCP server.
func NewTCPServer(port uint16, prefix string, enableTLS bool, rootDir string) (*TCPServer, error) {
	log.Printf("Tcp server listening on port %v\n", port)
	var lis net.Listener
	if enableTLS {
		certificate, err := tls.LoadX509KeyPair(
			filepath.Join(rootDir, "testdata/certs/cert-chain.pem"),
			filepath.Join(rootDir, "testdata/certs/key.pem"))
		if err != nil {
			return nil, err
		}
		caCert, err := ioutil.ReadFile(filepath.Join(rootDir, "testdata/certs/root-cert.pem"))
		if err != nil {
			return nil, err
		}
		caCertPool := x509.NewCertPool()
		caCertPool.AppendCertsFromPEM(caCert)

		config := &tls.Config{
			Certificates: []tls.Certificate{certificate},
			NextProtos:   []string{"istio2"},
			ClientAuth:   tls.RequestClientCert,
			ClientCAs:    caCertPool,
			ServerName:   "localhost",
		}
		lis, err = tls.Listen("tcp", fmt.Sprintf(":%d", port), config)
		if err != nil {
			log.Fatal(err)
			return nil, err
		}
	} else {
		var err error
		lis, err = net.Listen("tcp", fmt.Sprintf(":%d", port))
		if err != nil {
			log.Fatal(err)
			return nil, err
		}
	}

	return &TCPServer{
		port:      port,
		lis:       lis,
		prefix:    prefix,
		enableTLS: enableTLS,
		dir:       rootDir,
	}, nil
}

// handleConnection handles the lifetime of a connection
func handleConnection(conn net.Conn, prefix string) {
	defer conn.Close()
	reader := bufio.NewReader(conn)
	for {
		// read client request data
		bytes, err := reader.ReadBytes(byte('\n'))
		if err != nil {
			if err != io.EOF {
				log.Println("failed to read data, err:", err)
			}
			return
		}
		log.Printf("request: %s", bytes)

		// prepend prefix and send as response
		line := fmt.Sprintf("%s %s", prefix, bytes)
		log.Printf("response: %s", line)
		conn.Write([]byte(line))
	}
}

// WaitForTCPServer waits for a TCP server
func WaitForTCPServer(port uint16, enableTLS bool, rootDir string) error {
	var config *tls.Config

	if enableTLS {
		certPool := x509.NewCertPool()
		bs, err := ioutil.ReadFile(filepath.Join(rootDir, "testdata/certs/cert-chain.pem"))
		if err != nil {
			return fmt.Errorf("failed to read client ca cert: %s", err)
		}
		ok := certPool.AppendCertsFromPEM(bs)
		if !ok {
			return fmt.Errorf("failed to append client certs")
		}
		config = &tls.Config{RootCAs: certPool, NextProtos: []string{"istio2"}, ServerName: "localhost"}
	}
	for i := 0; i < maxAttempts; i++ {
		var conn net.Conn
		var err error
		if enableTLS {
			conn, err = tls.Dial("tcp", fmt.Sprintf("127.0.0.1:%d", port), config)
		} else {
			conn, err = net.Dial("tcp", fmt.Sprintf("127.0.0.1:%d", port))
		}
		if err != nil {
			log.Println("Will wait 200ms and try again.")
			time.Sleep(200 * time.Millisecond)
			continue
		}
		// send to socket
		fmt.Fprintf(conn, "ping"+"\n")
		// listen for reply
		message, err := bufio.NewReader(conn).ReadString('\n')
		if err != nil {
			log.Println("Will wait 200ms and try again.")
			time.Sleep(200 * time.Millisecond)
			continue
		}
		fmt.Print("Message from server: " + message)
		return nil
	}
	return fmt.Errorf("timeout waiting for server startup")
}

// Serve tcp requests
func Serve(l net.Listener, prefix string) error {
	for {
		conn, err := l.Accept()
		if err != nil {
			return fmt.Errorf("failed to accept connection, err:%v", err)
		}

		// pass an accepted connection to a handler goroutine
		go handleConnection(conn, prefix)
	}
}

// Start starts the server
func (s *TCPServer) Start() <-chan error {
	errCh := make(chan error)
	go func() {
		errCh <- Serve(s.lis, s.prefix)
	}()
	go func() {
		errCh <- WaitForTCPServer(s.port, s.enableTLS, s.dir)
	}()

	return errCh
}

// Stop shutdown the server
func (s *TCPServer) Stop() {
	log.Printf("Close TCP server\n")
	_ = s.lis.Close()
	log.Printf("Close TCP server -- Done\n")
}
