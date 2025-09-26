package main

import (
	"testing"

	pb "google.golang.org/grpc/examples/route_guide/routeguide"
)

// TestFeature is a bit contrived, but we're mostly interested that the proto
// compiled, could be imported and used within another library.
func TestFeature(t *testing.T) {
	name := "foo"
	f := pb.Feature{
		Name: name,
	}
	if f.Name != name {
		t.Fatalf(`Want "%s", got "%s"`, name, f.Name)
	}
}
