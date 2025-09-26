#include "quiche/balsa/balsa_headers_sequence.h"

#include <memory>
#include <utility>

#include "quiche/balsa/balsa_headers.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace quiche {
namespace test {
namespace {

TEST(BalsaHeadersSequenceTest, Initial) {
  BalsaHeadersSequence sequence;
  EXPECT_FALSE(sequence.HasNext());
  EXPECT_EQ(sequence.Next(), nullptr);
  EXPECT_TRUE(sequence.IsEmpty());
}

TEST(BalsaHeadersSequenceTest, Basic) {
  BalsaHeadersSequence sequence;

  auto headers_one = std::make_unique<BalsaHeaders>();
  headers_one->AppendHeader("one", "fish");
  sequence.Append(std::move(headers_one));
  EXPECT_TRUE(sequence.HasNext());
  EXPECT_FALSE(sequence.IsEmpty());

  auto headers_two = std::make_unique<BalsaHeaders>();
  headers_two->AppendHeader("two", "fish");
  sequence.Append(std::move(headers_two));
  EXPECT_TRUE(sequence.HasNext());
  EXPECT_FALSE(sequence.IsEmpty());

  const BalsaHeaders* headers = sequence.Next();
  ASSERT_NE(headers, nullptr);
  EXPECT_TRUE(headers->HasHeader("one"));
  EXPECT_TRUE(sequence.HasNext());
  EXPECT_FALSE(sequence.IsEmpty());

  headers = sequence.Next();
  ASSERT_NE(headers, nullptr);
  EXPECT_TRUE(headers->HasHeader("two"));
  EXPECT_FALSE(sequence.HasNext());
  EXPECT_FALSE(sequence.IsEmpty());

  EXPECT_EQ(sequence.Next(), nullptr);
}

TEST(BalsaHeadersSequenceTest, Clear) {
  BalsaHeadersSequence sequence;

  auto headers_one = std::make_unique<BalsaHeaders>();
  headers_one->AppendHeader("one", "fish");
  sequence.Append(std::move(headers_one));
  EXPECT_TRUE(sequence.HasNext());
  EXPECT_FALSE(sequence.IsEmpty());

  auto headers_two = std::make_unique<BalsaHeaders>();
  headers_two->AppendHeader("two", "fish");
  sequence.Append(std::move(headers_two));
  EXPECT_TRUE(sequence.HasNext());
  EXPECT_FALSE(sequence.IsEmpty());

  sequence.Clear();
  EXPECT_FALSE(sequence.HasNext());
  EXPECT_EQ(sequence.Next(), nullptr);
  EXPECT_TRUE(sequence.IsEmpty());
}

TEST(BalsaHeadersSequenceTest, PeekNext) {
  BalsaHeadersSequence sequence;
  EXPECT_EQ(sequence.PeekNext(), nullptr);

  auto headers_one = std::make_unique<BalsaHeaders>();
  headers_one->AppendHeader("one", "fish");
  sequence.Append(std::move(headers_one));
  EXPECT_TRUE(sequence.HasNext());

  const BalsaHeaders* headers = sequence.PeekNext();
  ASSERT_NE(headers, nullptr);
  EXPECT_TRUE(headers->HasHeader("one"));
  EXPECT_TRUE(sequence.HasNext());

  // Continuing to peek should not advance the sequence.
  EXPECT_EQ(sequence.PeekNext(), headers);

  // Adding more headers should not matter for peeking.
  auto headers_two = std::make_unique<BalsaHeaders>();
  headers_two->AppendHeader("two", "fish");
  sequence.Append(std::move(headers_two));
  EXPECT_TRUE(sequence.HasNext());
  EXPECT_EQ(sequence.PeekNext(), headers);

  headers = sequence.Next();
  ASSERT_NE(headers, nullptr);
  EXPECT_TRUE(headers->HasHeader("one"));
  EXPECT_TRUE(sequence.HasNext());

  headers = sequence.PeekNext();
  ASSERT_NE(headers, nullptr);
  EXPECT_TRUE(headers->HasHeader("two"));
  EXPECT_TRUE(sequence.HasNext());

  headers = sequence.Next();
  ASSERT_NE(headers, nullptr);
  EXPECT_TRUE(headers->HasHeader("two"));
  EXPECT_FALSE(sequence.HasNext());

  EXPECT_EQ(sequence.PeekNext(), nullptr);
}

TEST(BalsaHeadersSequenceTest, CanRetainValidReference) {
  BalsaHeadersSequence sequence;

  auto headers = std::make_unique<BalsaHeaders>();
  headers->AppendHeader("one", "fish");

  // This reference should still be valid, even after transferring ownership to
  // the sequence.
  BalsaHeaders* headers_ptr = headers.get();

  sequence.Append(std::move(headers));
  ASSERT_TRUE(sequence.HasNext());
  EXPECT_EQ(sequence.Next(), headers_ptr);
}

}  // namespace
}  // namespace test
}  // namespace quiche
