#include <assert.h>
#include <stdint.h>

extern int32_t return_5_in_no_std();

int main(int argc, char** argv) {
    assert(return_5_in_no_std() == 5);
    return 0;
}
