#include "quiche/http2/test_tools/random_decoder_test_base.h"

#include <stddef.h>

#include <functional>
#include <ios>
#include <set>
#include <type_traits>

#include "quiche/http2/decoder/decode_buffer.h"
#include "quiche/http2/decoder/decode_status.h"
#include "quiche/http2/test_tools/http2_random.h"
#include "quiche/common/platform/api/quiche_logging.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/quiche_callbacks.h"

namespace http2 {
namespace test {
namespace {
const char kData[]{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
const bool kReturnNonZeroOnFirst = true;
const bool kMayReturnZeroOnFirst = false;

// Confirm the behavior of various parts of RandomDecoderTest.
class RandomDecoderTestTest : public RandomDecoderTest {
 public:
  RandomDecoderTestTest() : data_db_(kData) {
    QUICHE_CHECK_EQ(sizeof kData, 8u);
  }

 protected:
  typedef quiche::MultiUseCallback<DecodeStatus(DecodeBuffer* db)> DecodingFn;

  DecodeStatus StartDecoding(DecodeBuffer* db) override {
    ++start_decoding_calls_;
    if (start_decoding_fn_) {
      return start_decoding_fn_(db);
    }
    return DecodeStatus::kDecodeError;
  }

  DecodeStatus ResumeDecoding(DecodeBuffer* db) override {
    ++resume_decoding_calls_;
    if (resume_decoding_fn_) {
      return resume_decoding_fn_(db);
    }
    return DecodeStatus::kDecodeError;
  }

  bool StopDecodeOnDone() override {
    ++stop_decode_on_done_calls_;
    if (override_stop_decode_on_done_) {
      return sub_stop_decode_on_done_;
    }
    return RandomDecoderTest::StopDecodeOnDone();
  }

  size_t start_decoding_calls_ = 0;
  size_t resume_decoding_calls_ = 0;
  size_t stop_decode_on_done_calls_ = 0;

  DecodingFn start_decoding_fn_;
  DecodingFn resume_decoding_fn_;

  DecodeBuffer data_db_;

  bool sub_stop_decode_on_done_ = true;
  bool override_stop_decode_on_done_ = true;
};

// Decode a single byte on the StartDecoding call, then stop.
TEST_F(RandomDecoderTestTest, StopOnStartPartiallyDone) {
  start_decoding_fn_ = [this](DecodeBuffer* db) {
    EXPECT_EQ(1u, start_decoding_calls_);
    // Make sure the correct buffer is being used.
    EXPECT_EQ(kData, db->cursor());
    EXPECT_EQ(sizeof kData, db->Remaining());
    db->DecodeUInt8();
    return DecodeStatus::kDecodeDone;
  };

  EXPECT_EQ(DecodeStatus::kDecodeDone,
            DecodeSegments(&data_db_, SelectRemaining()));
  EXPECT_EQ(1u, data_db_.Offset());
  // StartDecoding should only be called once from each call to DecodeSegments.
  EXPECT_EQ(1u, start_decoding_calls_);
  EXPECT_EQ(0u, resume_decoding_calls_);
  EXPECT_EQ(1u, stop_decode_on_done_calls_);
}

// Stop decoding upon return from the first ResumeDecoding call.
TEST_F(RandomDecoderTestTest, StopOnResumePartiallyDone) {
  start_decoding_fn_ = [this](DecodeBuffer* db) {
    EXPECT_EQ(1u, start_decoding_calls_);
    db->DecodeUInt8();
    return DecodeStatus::kDecodeInProgress;
  };
  resume_decoding_fn_ = [this](DecodeBuffer* db) {
    EXPECT_EQ(1u, resume_decoding_calls_);
    // Make sure the correct buffer is being used.
    EXPECT_EQ(data_db_.cursor(), db->cursor());
    db->DecodeUInt16();
    return DecodeStatus::kDecodeDone;
  };

  // Check that the base class honors it's member variable stop_decode_on_done_.
  override_stop_decode_on_done_ = false;
  stop_decode_on_done_ = true;

  EXPECT_EQ(DecodeStatus::kDecodeDone,
            DecodeSegments(&data_db_, SelectRemaining()));
  EXPECT_EQ(3u, data_db_.Offset());
  EXPECT_EQ(1u, start_decoding_calls_);
  EXPECT_EQ(1u, resume_decoding_calls_);
  EXPECT_EQ(1u, stop_decode_on_done_calls_);
}

// Decode a random sized chunks, always reporting back kDecodeInProgress.
TEST_F(RandomDecoderTestTest, InProgressWhenEmpty) {
  start_decoding_fn_ = [this](DecodeBuffer* db) {
    EXPECT_EQ(1u, start_decoding_calls_);
    // Consume up to 2 bytes.
    if (db->HasData()) {
      db->DecodeUInt8();
      if (db->HasData()) {
        db->DecodeUInt8();
      }
    }
    return DecodeStatus::kDecodeInProgress;
  };
  resume_decoding_fn_ = [](DecodeBuffer* db) {
    // Consume all available bytes.
    if (db->HasData()) {
      db->AdvanceCursor(db->Remaining());
    }
    return DecodeStatus::kDecodeInProgress;
  };

  EXPECT_EQ(DecodeStatus::kDecodeInProgress,
            DecodeSegments(&data_db_, SelectRandom(kMayReturnZeroOnFirst)));
  EXPECT_TRUE(data_db_.Empty());
  EXPECT_EQ(1u, start_decoding_calls_);
  EXPECT_LE(1u, resume_decoding_calls_);
  EXPECT_EQ(0u, stop_decode_on_done_calls_);
}

TEST_F(RandomDecoderTestTest, DoneExactlyAtEnd) {
  start_decoding_fn_ = [this](DecodeBuffer* db) {
    EXPECT_EQ(1u, start_decoding_calls_);
    EXPECT_EQ(1u, db->Remaining());
    EXPECT_EQ(1u, db->FullSize());
    db->DecodeUInt8();
    return DecodeStatus::kDecodeInProgress;
  };
  resume_decoding_fn_ = [this](DecodeBuffer* db) {
    EXPECT_EQ(1u, db->Remaining());
    EXPECT_EQ(1u, db->FullSize());
    db->DecodeUInt8();
    if (data_db_.Remaining() == 1) {
      return DecodeStatus::kDecodeDone;
    }
    return DecodeStatus::kDecodeInProgress;
  };
  override_stop_decode_on_done_ = true;
  sub_stop_decode_on_done_ = true;

  EXPECT_EQ(DecodeStatus::kDecodeDone, DecodeSegments(&data_db_, SelectOne()));
  EXPECT_EQ(0u, data_db_.Remaining());
  EXPECT_EQ(1u, start_decoding_calls_);
  EXPECT_EQ((sizeof kData) - 1, resume_decoding_calls_);
  // Didn't need to call StopDecodeOnDone because we didn't finish early.
  EXPECT_EQ(0u, stop_decode_on_done_calls_);
}

TEST_F(RandomDecoderTestTest, DecodeSeveralWaysToEnd) {
  // Each call to StartDecoding or ResumeDecoding will consume all that is
  // available. When all the data has been consumed, returns kDecodeDone.
  size_t decoded_since_start = 0;
  auto shared_fn = [&decoded_since_start, this](DecodeBuffer* db) {
    decoded_since_start += db->Remaining();
    db->AdvanceCursor(db->Remaining());
    EXPECT_EQ(0u, db->Remaining());
    if (decoded_since_start == data_db_.FullSize()) {
      return DecodeStatus::kDecodeDone;
    }
    return DecodeStatus::kDecodeInProgress;
  };

  start_decoding_fn_ = [&decoded_since_start, shared_fn](DecodeBuffer* db) {
    decoded_since_start = 0;
    return shared_fn(db);
  };
  resume_decoding_fn_ = shared_fn;

  Validator validator = ValidateDoneAndEmpty();

  EXPECT_TRUE(DecodeAndValidateSeveralWays(&data_db_, kMayReturnZeroOnFirst,
                                           validator));

  // We should have reached the end.
  EXPECT_EQ(0u, data_db_.Remaining());

  // We currently have 4 ways of decoding; update this if that changes.
  EXPECT_EQ(4u, start_decoding_calls_);

  // Didn't need to call StopDecodeOnDone because we didn't finish early.
  EXPECT_EQ(0u, stop_decode_on_done_calls_);
}

TEST_F(RandomDecoderTestTest, DecodeTwoWaysAndStopEarly) {
  // On the second decode, return kDecodeDone before finishing.
  size_t decoded_since_start = 0;
  auto shared_fn = [&decoded_since_start, this](DecodeBuffer* db) {
    uint32_t amount = db->Remaining();
    if (start_decoding_calls_ == 2 && amount > 1) {
      amount = 1;
    }
    decoded_since_start += amount;
    db->AdvanceCursor(amount);
    if (decoded_since_start == data_db_.FullSize()) {
      return DecodeStatus::kDecodeDone;
    }
    if (decoded_since_start > 1 && start_decoding_calls_ == 2) {
      return DecodeStatus::kDecodeDone;
    }
    return DecodeStatus::kDecodeInProgress;
  };

  start_decoding_fn_ = [&decoded_since_start, shared_fn](DecodeBuffer* db) {
    decoded_since_start = 0;
    return shared_fn(db);
  };
  resume_decoding_fn_ = shared_fn;

  // We expect the first and second to succeed, but the second to end at a
  // different offset, which DecodeAndValidateSeveralWays should complain about.
  Validator validator = [this](const DecodeBuffer& /*input*/,
                               DecodeStatus status) -> AssertionResult {
    if (start_decoding_calls_ <= 2 && status != DecodeStatus::kDecodeDone) {
      return ::testing::AssertionFailure()
             << "Expected DecodeStatus::kDecodeDone, not " << status;
    }
    if (start_decoding_calls_ > 2) {
      return ::testing::AssertionFailure()
             << "How did we get to pass " << start_decoding_calls_;
    }
    return ::testing::AssertionSuccess();
  };

  EXPECT_FALSE(DecodeAndValidateSeveralWays(&data_db_, kMayReturnZeroOnFirst,
                                            validator));
  EXPECT_EQ(2u, start_decoding_calls_);
  EXPECT_EQ(1u, stop_decode_on_done_calls_);
}

TEST_F(RandomDecoderTestTest, DecodeThreeWaysAndError) {
  // Return kDecodeError from ResumeDecoding on the third decoding pass.
  size_t decoded_since_start = 0;
  auto shared_fn = [&decoded_since_start, this](DecodeBuffer* db) {
    if (start_decoding_calls_ == 3 && decoded_since_start > 0) {
      return DecodeStatus::kDecodeError;
    }
    uint32_t amount = db->Remaining();
    if (start_decoding_calls_ == 3 && amount > 1) {
      amount = 1;
    }
    decoded_since_start += amount;
    db->AdvanceCursor(amount);
    if (decoded_since_start == data_db_.FullSize()) {
      return DecodeStatus::kDecodeDone;
    }
    return DecodeStatus::kDecodeInProgress;
  };

  start_decoding_fn_ = [&decoded_since_start, shared_fn](DecodeBuffer* db) {
    decoded_since_start = 0;
    return shared_fn(db);
  };
  resume_decoding_fn_ = shared_fn;

  Validator validator = ValidateDoneAndEmpty();
  EXPECT_FALSE(DecodeAndValidateSeveralWays(&data_db_, kReturnNonZeroOnFirst,
                                            validator));
  EXPECT_EQ(3u, start_decoding_calls_);
  EXPECT_EQ(0u, stop_decode_on_done_calls_);
}

// CorruptEnum should produce lots of different values. On the assumption that
// the enum gets at least a byte of storage, we should be able to produce
// 256 distinct values.
TEST(CorruptEnumTest, ManyValues) {
  std::set<uint64_t> values;
  DecodeStatus status;
  QUICHE_LOG(INFO) << "sizeof status = " << sizeof status;
  Http2Random rng;
  for (int ndx = 0; ndx < 256; ++ndx) {
    CorruptEnum(&status, &rng);
    values.insert(static_cast<uint64_t>(status));
  }
}

// In practice the underlying type is an int, and currently that is 4 bytes.
typedef typename std::underlying_type<DecodeStatus>::type DecodeStatusUT;

struct CorruptEnumTestStruct {
  DecodeStatusUT filler1;
  DecodeStatus status;
  DecodeStatusUT filler2;
};

// CorruptEnum should only overwrite the enum, not any adjacent storage.
TEST(CorruptEnumTest, CorruptsOnlyEnum) {
  Http2Random rng;
  for (const DecodeStatusUT filler : {DecodeStatusUT(), ~DecodeStatusUT()}) {
    QUICHE_LOG(INFO) << "filler=0x" << std::hex << filler;
    CorruptEnumTestStruct s;
    s.filler1 = filler;
    s.filler2 = filler;
    for (int ndx = 0; ndx < 256; ++ndx) {
      CorruptEnum(&s.status, &rng);
      EXPECT_EQ(s.filler1, filler);
      EXPECT_EQ(s.filler2, filler);
    }
  }
}

}  // namespace
}  // namespace test
}  // namespace http2
