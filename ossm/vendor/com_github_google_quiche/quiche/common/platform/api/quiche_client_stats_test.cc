#include "quiche/common/platform/api/quiche_client_stats.h"

#include "quiche/quic/core/quic_time.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace quiche::test {

enum class TestEnum { ZERO = 0, ONE, TWO, COUNT };

TEST(QuichePlatformTest, QuicheClientStats) {
  // Just make sure they compile.
  QUICHE_CLIENT_HISTOGRAM_ENUM("my.enum.histogram", TestEnum::ONE,
                               TestEnum::COUNT, "doc");
  QUICHE_CLIENT_HISTOGRAM_BOOL("my.bool.histogram", false, "doc");
  QUICHE_CLIENT_HISTOGRAM_TIMES(
      "my.timing.histogram", quic::QuicTime::Delta::FromSeconds(5),
      quic::QuicTime::Delta::FromSeconds(1),
      quic::QuicTime::Delta::FromSeconds(3600), 100, "doc");
  QUICHE_CLIENT_HISTOGRAM_COUNTS("my.count.histogram", 123, 0, 1000, 100,
                                 "doc");
  std::string histogram_name = "my.sparse.histogram";
  int value = 345;
  QuicheClientSparseHistogramImpl(histogram_name, value);
  // Make sure compiler doesn't report unused-parameter error.
  bool should_be_used = false;
  QUICHE_CLIENT_HISTOGRAM_BOOL_IMPL("my.bool.histogram", should_be_used, "doc");
}

}  // namespace quiche::test
