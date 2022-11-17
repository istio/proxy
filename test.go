package main

import (
	"bytes"
	"io"
	"log"
	"net"

	proxyproto "github.com/pires/go-proxyproto"
)

func chkErr(err error) {
	if err != nil {
		log.Fatalf("Error: %s", err.Error())
	}
}

func main() {
	// Dial some proxy listener e.g. https://github.com/mailgun/proxyproto
	target, err := net.ResolveTCPAddr("tcp", "127.0.0.1:8000")
	chkErr(err)

	conn, err := net.DialTCP("tcp", nil, target)
	chkErr(err)
	defer conn.Close()

	// Create a proxyprotocol header or use HeaderProxyFromAddrs() if you
	// have two conn's
	header := &proxyproto.Header{
		Version:           2,
		Command:           proxyproto.PROXY,
		TransportProtocol: proxyproto.TCPv4,
		SourceAddr: &net.TCPAddr{
			IP:   net.ParseIP("10.1.1.1"),
			Port: 1000,
		},
		DestinationAddr: &net.TCPAddr{
			IP:   net.ParseIP("20.2.2.2"),
			Port: 2000,
		},
	}
	header.SetTLVs([]proxyproto.TLV{{
		Type:  proxyproto.PP2Type(64),
		Value: []byte("client_identity"),
	}})
	// After the connection was created write the proxy headers first
	_, err = header.WriteTo(conn)
	chkErr(err)
	// Then your data... e.g.:
	_, err = io.WriteString(conn, "GET / HTTP/1.1\r\nHost: envoy.com\r\nConnection: close\r\n\r\n")
	chkErr(err)
	var buf bytes.Buffer
	io.Copy(&buf, conn)
	log.Print(buf.String())
}
