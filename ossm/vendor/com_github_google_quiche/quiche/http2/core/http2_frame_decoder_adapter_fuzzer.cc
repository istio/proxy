#include "testing/fuzzing/fuzztest.h"
#include "quiche/http2/core/http2_frame_decoder_adapter.h"
#include "quiche/http2/core/spdy_no_op_visitor.h"

void DecoderFuzzTest(const std::string& data) {
  spdy::SpdyNoOpVisitor visitor;
  http2::Http2DecoderAdapter decoder;
  decoder.set_visitor(&visitor);
  decoder.ProcessInput(data.data(), data.size());
}
FUZZ_TEST(Http2FrameDecoderAdapterFuzzTest, DecoderFuzzTest);
