package main

import (
    "fmt"

    pb "github.com/rules-proto-grpc/rules_proto_grpc/test_workspaces/go_fixer"
)

func main() {
    test := &pb.Demo{
        Field: true,
    }
    fmt.Printf("%v\n", test.GetField())
}