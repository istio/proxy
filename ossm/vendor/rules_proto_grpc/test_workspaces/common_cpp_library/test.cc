#include "common.pb.h"
#include "a.pb.h"
#include "b.pb.h"

int main () {
    message_a().ByteSizeLong();
    message_b().ByteSizeLong();
    return CommonProto().ByteSizeLong();
}
