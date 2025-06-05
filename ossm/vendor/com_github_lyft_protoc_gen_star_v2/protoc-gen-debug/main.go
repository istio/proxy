// protoc-gen-debug emits the raw encoded CodeGeneratorRequest from a protoc
// execution to a file. This is particularly useful for testing (see the
// testdata/graph package for test cases).
package main

import (
	"bytes"
	"io"
	"io/ioutil"
	"log"
	"os"
	"path/filepath"

	"google.golang.org/protobuf/types/pluginpb"

	"google.golang.org/protobuf/proto"
	plugin_go "google.golang.org/protobuf/types/pluginpb"
)

func main() {
	data, err := ioutil.ReadAll(os.Stdin)
	if err != nil {
		log.Fatal("unable to read input: ", err)
	}

	req := &plugin_go.CodeGeneratorRequest{}
	if err = proto.Unmarshal(data, req); err != nil {
		log.Fatal("unable to unmarshal request: ", err)
	}

	path := req.GetParameter()
	if path == "" {
		log.Fatal(`please execute the plugin with the output path to properly write the output file: --debug_out="{PATH}:{PATH}"`)
	}

	err = os.MkdirAll(path, 0755)
	if err != nil {
		log.Fatal("unable to create output dir: ", err)
	}

	err = ioutil.WriteFile(filepath.Join(path, "code_generator_request.pb.bin"), data, 0644)
	if err != nil {
		log.Fatal("unable to write request to disk: ", err)
	}

	// protoc-gen-debug supports proto3 field presence for testing purposes
	var supportedFeatures = uint64(pluginpb.CodeGeneratorResponse_FEATURE_PROTO3_OPTIONAL)
	if data, err = proto.Marshal(&plugin_go.CodeGeneratorResponse{
		SupportedFeatures: &supportedFeatures,
	}); err != nil {
		log.Fatal("unable to marshal response payload: ", err)
	}

	_, err = io.Copy(os.Stdout, bytes.NewReader(data))
	if err != nil {
		log.Fatal("unable to write response to stdout: ", err)
	}
}
