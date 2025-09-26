#include <assert.h>
#include <stdint.h>

extern "C" int32_t four();

int main(int argc, char** argv) {
    assert(four() == 4);
    return 0;
}
