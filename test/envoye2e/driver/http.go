package driver

import (
	"fmt"
	"net/http"
	"time"
)

type HTTPServer struct {
}

var _ Step = &HTTPServer{}

func (s *HTTPServer) Run(p *Params) error {
	fmt.Printf("Listening on %d\n", p.Ports.BackendPort)
	http.HandleFunc("/", s.ping)
	http.HandleFunc("/close", s.close)
	go http.ListenAndServe(fmt.Sprintf(":%d", p.Ports.BackendPort), nil)
	return nil
}

func (s *HTTPServer) ping(w http.ResponseWriter, req *http.Request) {
	w.WriteHeader(http.StatusOK)
	return
}

func (s *HTTPServer) close(w http.ResponseWriter, req *http.Request) {
	time.Sleep(3 * time.Second)
	w.Header().Set("Connection", "close")
	return
}

func (s *HTTPServer) Cleanup() {}
