package objc

/*
#include "add.h"
*/
import "C"

func AddC(a, b int32) int32 {
	return int32(C.add_c(C.int(a), C.int(b)))
}

func AddCPP(a, b int32) int32 {
	return int32(C.add_cpp(C.int(a), C.int(b)))
}
