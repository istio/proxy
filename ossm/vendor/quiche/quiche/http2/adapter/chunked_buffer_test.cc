#include "quiche/http2/adapter/chunked_buffer.h"

#include <algorithm>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>

#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace http2 {
namespace adapter {
namespace {

constexpr absl::string_view kLoremIpsum =
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod "
    "tempor incididunt ut labore et dolore magna aliqua.";

struct DataAndSize {
  std::unique_ptr<char[]> data;
  size_t size;
};

DataAndSize MakeDataAndSize(absl::string_view source) {
  auto data = std::unique_ptr<char[]>(new char[source.size()]);
  std::copy(source.begin(), source.end(), data.get());
  return {std::move(data), source.size()};
}

TEST(ChunkedBufferTest, Empty) {
  ChunkedBuffer buffer;
  EXPECT_TRUE(buffer.Empty());

  buffer.Append("some data");
  EXPECT_FALSE(buffer.Empty());

  buffer.RemovePrefix(9);
  EXPECT_TRUE(buffer.Empty());
}

TEST(ChunkedBufferTest, ReusedAfterEmptied) {
  ChunkedBuffer buffer;
  buffer.Append("some data");
  buffer.RemovePrefix(9);
  buffer.Append("different data");
  EXPECT_EQ("different data", buffer.GetPrefix());
}

TEST(ChunkedBufferTest, LargeAppendAfterEmptied) {
  ChunkedBuffer buffer;
  buffer.Append("some data");
  EXPECT_THAT(buffer.GetPrefix(), testing::StartsWith("some data"));
  buffer.RemovePrefix(9);
  auto more_data =
      MakeDataAndSize(absl::StrCat("different data", std::string(2048, 'x')));
  buffer.Append(std::move(more_data.data), more_data.size);
  EXPECT_THAT(buffer.GetPrefix(), testing::StartsWith("different data"));
}

TEST(ChunkedBufferTest, LargeAppends) {
  ChunkedBuffer buffer;
  buffer.Append(std::string(500, 'a'));
  buffer.Append(std::string(2000, 'b'));
  buffer.Append(std::string(10, 'c'));
  auto more_data = MakeDataAndSize(std::string(4490, 'd'));
  buffer.Append(std::move(more_data.data), more_data.size);

  EXPECT_EQ(500 + 2000 + 10 + 4490, absl::StrJoin(buffer.Read(), "").size());
}

TEST(ChunkedBufferTest, RemovePartialPrefix) {
  ChunkedBuffer buffer;
  auto data_and_size = MakeDataAndSize(kLoremIpsum);
  buffer.Append(std::move(data_and_size.data), data_and_size.size);
  EXPECT_EQ(kLoremIpsum.size(), buffer.TotalSize());
  buffer.RemovePrefix(6);
  EXPECT_THAT(buffer.GetPrefix(), testing::StartsWith("ipsum"));
  EXPECT_EQ(kLoremIpsum.size() - 6, buffer.TotalSize());
  buffer.RemovePrefix(20);
  EXPECT_THAT(buffer.GetPrefix(), testing::StartsWith(", consectetur"));
  EXPECT_EQ(kLoremIpsum.size() - 26, buffer.TotalSize());
  buffer.Append(" Anday igpay atinlay!");
  EXPECT_EQ(kLoremIpsum.size() - 26 + 21, buffer.TotalSize());
  const std::initializer_list<absl::string_view> parts = {
      kLoremIpsum.substr(26), " Anday igpay atinlay!"};
  EXPECT_EQ(absl::StrJoin(parts, ""), absl::StrJoin(buffer.Read(), ""));
}

TEST(ChunkedBufferTest, DifferentAppends) {
  ChunkedBuffer buffer;
  buffer.Append("Lorem ipsum");

  auto more_data = MakeDataAndSize(" dolor sit amet, ");
  buffer.Append(std::move(more_data.data), more_data.size);

  buffer.Append("consectetur adipiscing elit, ");

  more_data = MakeDataAndSize("sed do eiusmod tempor incididunt ut ");
  buffer.Append(std::move(more_data.data), more_data.size);

  buffer.Append("labore et dolore magna aliqua.");

  EXPECT_EQ(kLoremIpsum, absl::StrJoin(buffer.Read(), ""));

  buffer.RemovePrefix(kLoremIpsum.size());
  EXPECT_TRUE(buffer.Empty());
}

TEST(ChunkedBufferTest, GetAppendRegion) {
  ChunkedBuffer buffer;
  constexpr absl::string_view kFirstAppendText = "Lorem ipsum";
  {
    auto region = buffer.GetAppendRegion();
    ASSERT_GE(region.size, kFirstAppendText.size());
    memcpy(region.data, kFirstAppendText.data(), kFirstAppendText.size());
    region.written = kFirstAppendText.size();
  }
  EXPECT_EQ(kFirstAppendText, buffer.GetPrefix());
  EXPECT_EQ(kFirstAppendText.size(), buffer.TotalSize());

  constexpr absl::string_view kSecondAppendText = " dolor sit amet, ";
  {
    auto region = buffer.GetAppendRegion();
    ASSERT_GE(region.size, kSecondAppendText.size());
    memcpy(region.data, kSecondAppendText.data(), kSecondAppendText.size());
    region.written = kSecondAppendText.size();
  }
  EXPECT_EQ(kFirstAppendText.size() + kSecondAppendText.size(),
            buffer.TotalSize());
  EXPECT_EQ(absl::StrCat(kFirstAppendText, kSecondAppendText),
            buffer.GetPrefix());
}

TEST(ChunkedBufferTest, GetAppendRegionAcrossChunks) {
  ChunkedBuffer buffer;
  {
    auto region = buffer.GetAppendRegion();
    ASSERT_GE(region.size, 1024);
    const std::string first_part(1024, 'a');
    memcpy(region.data, first_part.data(), 1024);
    region.written = 1024;
  }
  EXPECT_EQ(1024, buffer.TotalSize());
  {
    auto region = buffer.GetAppendRegion();
    ASSERT_GE(region.size, 1024);
    const std::string second_part(10, 'b');
    memcpy(region.data, second_part.data(), 10);
    region.written = 10;
  }
  EXPECT_EQ(1034, buffer.TotalSize());
  const std::string expected =
      absl::StrCat(std::string(1024, 'a'), std::string(10, 'b'));
  EXPECT_EQ(expected, absl::StrJoin(buffer.Read(), ""));
}

TEST(ChunkedBufferTest, GetAppendRegionThenAppend) {
  ChunkedBuffer buffer;
  constexpr absl::string_view kFirstAppendText = "Lorem ipsum";
  {
    auto region = buffer.GetAppendRegion();
    ASSERT_GE(region.size, kFirstAppendText.size());
    memcpy(region.data, kFirstAppendText.data(), kFirstAppendText.size());
    region.written = kFirstAppendText.size();
  }
  constexpr absl::string_view kSecondAppendText = " dolor sit amet, ";
  buffer.Append(kSecondAppendText);
  EXPECT_EQ(kFirstAppendText.size() + kSecondAppendText.size(),
            buffer.TotalSize());
  EXPECT_EQ(absl::StrCat(kFirstAppendText, kSecondAppendText),
            buffer.GetPrefix());
}

}  // namespace
}  // namespace adapter
}  // namespace http2
