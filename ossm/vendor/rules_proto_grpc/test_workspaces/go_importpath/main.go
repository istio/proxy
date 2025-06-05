package main

import (
    "fmt"

    pb_a "test_import_path/root"
    pb_b "test_import_path/folder"
)

func main() {
    test_a := &pb_a.Demo{
        Field: true,
    }
    test_b := &pb_b.NestedDemo{
        Field: true,
    }
    fmt.Printf("%v\n", test_a.GetField())
    fmt.Printf("%v\n", test_b.GetField())
}