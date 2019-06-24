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
	"fmt"
	"io"
	"log"
	"net"
	"time"
)

// TCPServer stores data for a TCP server.
type TCPServer struct {
	port   uint16
	lis    net.Listener
	prefix string
}

// NewTCPServer creates a new TCP server.
func NewTCPServer(port uint16, prefix string) (*TCPServer, error) {
	log.Printf("Tcp server listening on port %v\n", port)
	lis, err := net.Listen("tcp", fmt.Sprintf(":%d", port))
	if err != nil {
		log.Fatal(err)
		return nil, err
	}
	return &TCPServer{
		port:   port,
		lis:    lis,
		prefix: prefix,
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
func WaitForTCPServer(port uint16) error {
	for i := 0; i < maxAttempts; i++ {
		conn, err := net.Dial("tcp", fmt.Sprintf("127.0.0.1:%d", port))
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

func Serve(l net.Listener, prefix string) error {
	for {
		conn, err := l.Accept()
		if err != nil {
			return fmt.Errorf("failed to accept connection, err:", err)
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
		errCh <- WaitForTCPServer(s.port)
	}()

	return errCh
}

// Stop shutdown the server
func (s *TCPServer) Stop() {
	log.Printf("Close TCP server\n")
	_ = s.lis.Close()
	log.Printf("Close TCP server -- Done\n")
}
