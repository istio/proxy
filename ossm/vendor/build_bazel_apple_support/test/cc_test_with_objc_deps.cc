#include <stdio.h>
#include <stdlib.h>

#include "test/objc_lib.h"

int main() {
  if (from_objc_library() == 10) {
    printf("SUCCESS: from_objc_library() = %d\n", from_objc_library());
    return EXIT_SUCCESS;
  } else {
    printf("FAILED: from_objc_library() = %d, expected 10\n", from_objc_library());
    return EXIT_FAILURE;
  }
}
