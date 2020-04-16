// Copyright 2020 Istio Authors
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
	"bufio"
	"errors"
	"fmt"
	"io"
	"log"
	"net"
	"time"
)

type TCPServer struct {
	lis    net.Listener
	Prefix string
}

var _ Step = &TCPServer{}

func (t *TCPServer) Run(p *Params) error {
	var err error
	t.lis, err = net.Listen("tcp", fmt.Sprintf(":%d", p.Ports.BackendPort))
	if err != nil {
		return fmt.Errorf("failed to listen on %v", err)
	}
	go t.serve()
	if err = waitForTCPServer(p.Ports.BackendPort); err != nil {
		return err
	}
	return nil
}

func (t *TCPServer) Cleanup() {
	t.lis.Close()
}

func (t *TCPServer) serve() {
	for {
		conn, err := t.lis.Accept()
		if err != nil {
			return
		}

		// pass an accepted connection to a handler goroutine
		go handleConnection(conn, t.Prefix)
	}
}

func waitForTCPServer(port uint16) error {
	for i := 0; i < 30; i++ {
		var conn net.Conn
		var err error
		conn, err = net.Dial("tcp", fmt.Sprintf("127.0.0.1:%d", port))
		if err != nil {
			log.Println("Will wait 200ms and try again.")
			time.Sleep(200 * time.Millisecond)
			continue
		}
		// send to socket
		fmt.Fprintf(conn, "ping\n")
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
	return errors.New("timeout waiting for TCP server to be ready")
}

func handleConnection(conn net.Conn, prefix string) {
	defer conn.Close()
	reader := bufio.NewReader(conn)
	for {
		// read client request data
		bytes, err := reader.ReadString('\n')
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
		_, _ = conn.Write([]byte(line))
	}
}

type TCPConnection struct{}

var _ Step = &TCPConnection{}

func (t *TCPConnection) Run(p *Params) error {
	conn, err := net.Dial("tcp", fmt.Sprintf("127.0.0.1:%d", p.Ports.ClientPort))
	if err != nil {
		return fmt.Errorf("failed to connect to tcp server: %v", err)
	}
	defer conn.Close()
	// send to socket
	fmt.Fprintf(conn, "world"+"\n")
	// listen for reply
	message, err := bufio.NewReader(conn).ReadString('\n')
	if err != nil {
		return fmt.Errorf("failed to read bytes from conn %v", err)
	}
	wantMessage := "hello world\n"
	if message != wantMessage {
		return fmt.Errorf("received bytes got %v want %v", message, wantMessage)
	}
	return nil
}

func (t *TCPConnection) Cleanup() {}
