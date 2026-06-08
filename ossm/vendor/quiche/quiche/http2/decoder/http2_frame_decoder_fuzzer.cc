#include <string>

#include "absl/strings/string_view.h"
#include "quiche/http2/decoder/decode_buffer.h"
#include "quiche/http2/decoder/http2_frame_decoder.h"
#include "quiche/http2/decoder/http2_frame_decoder_listener.h"
#include "quiche/common/platform/api/quiche_fuzztest.h"

namespace http2 {
namespace {

void DoesNotCrash(const absl::string_view data) {
  Http2FrameDecoderNoOpListener listener;
  Http2FrameDecoder decoder(&listener);
  DecodeBuffer db(data.data(), data.size());
  decoder.DecodeFrame(&db);
}
FUZZ_TEST(Http2FrameDecoderFuzzer, DoesNotCrash)
    .WithDomains(fuzztest::Arbitrary<std::string>().WithMinSize(1));

}  // namespace
}  // namespace http2
