package main

// const char *msg = "cgo";
import "C"

import "fmt"

func main() {
	fmt.Println(C.GoString(C.msg))
}
