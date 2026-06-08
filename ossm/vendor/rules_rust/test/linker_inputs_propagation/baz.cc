#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>

extern "C" int32_t double_foo();

int main(int argc, char** argv) {
    assert(double_foo() == 84);
    return EXIT_SUCCESS;
}
