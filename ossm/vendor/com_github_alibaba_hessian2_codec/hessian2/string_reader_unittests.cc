#include <climits>
#include <vector>

#include "common/common.h"
#include "gtest/gtest.h"
#include "hessian2/reader.hpp"
#include "hessian2/string_reader.hpp"

namespace Hessian2 {

class StringReaderTest : public testing::Test {
 public:
  void addSeq(std::string buffer, const std::initializer_list<uint8_t> values) {
    for (int8_t value : values) {
      buffer.push_back(value);
    }
  }

  template <typename T>
  void readBE(std::vector<std::pair<T, bool>> &vals) {
    for (const auto &val : vals) {
      auto out = reader_.readBE<T>();
      EXPECT_EQ(std::get<1>(val), out.first);
      if (out.first) {
        EXPECT_EQ(std::get<0>(val), out.second);
      }
    }
  }

  template <typename T>
  void readLE(std::vector<std::pair<T, bool>> &vals) {
    for (const auto &val : vals) {
      auto out = reader_.readLE<T>();
      EXPECT_EQ(std::get<1>(val), out.first);
      if (out.first) {
        EXPECT_EQ(std::get<0>(val), out.second);
      }
    }
  }

  StringReader reader_{absl::string_view("")};
};

TEST_F(StringReaderTest, ReadAndPeekI8) {
  {
    unsigned char buf[] = {0, 1, 0XFE};
    absl::string_view buffer(reinterpret_cast<char *>(&buf), 3);
    reader_ = StringReader(buffer);

    EXPECT_EQ(reader_.peek<int8_t>().second, 0);
    EXPECT_EQ(reader_.peek<int8_t>(0).second, 0);
    EXPECT_EQ(reader_.peek<int8_t>(1).second, 1);
    EXPECT_EQ(reader_.peek<int8_t>(2).second, -2);
    EXPECT_EQ(buffer.size(), 3);

    std::vector<std::pair<int8_t, bool>> vals = {
        {0, true}, {1, true}, {-2, true}, {0, false}};
    readBE<int8_t>(vals);
    EXPECT_EQ(buffer.size(), 3);
  }

  {
    std::string input;
    reader_ = StringReader(input);
    std::vector<std::pair<int8_t, bool>> vals = {{0, false}};
    EXPECT_EQ(reader_.peek<int8_t>(0).first, false);
    readBE<int8_t>(vals);
  }

  {
    std::string input(1, 0x30);
    reader_ = StringReader(input);
    std::vector<std::pair<int8_t, bool>> vals = {{0x30, true}, {0, false}};
    EXPECT_EQ(reader_.peek<int8_t>(0).second, 0x30);
    EXPECT_EQ(reader_.peek<int8_t>(1).first, false);
    readBE<int8_t>(vals);
  }
}

TEST_F(StringReaderTest, ReadAndPeekLEI16) {
  {
    unsigned char buf[] = {0, 1, 2, 3, 0xFF, 0xFF};
    absl::string_view buffer(reinterpret_cast<char *>(&buf), 6);
    reader_ = StringReader(buffer);
    EXPECT_EQ(reader_.peekLE<int16_t>().second, 0x0100);
    EXPECT_EQ(reader_.peekLE<int16_t>(0).second, 0x0100);
    EXPECT_EQ(reader_.peekLE<int16_t>(1).second, 0x0201);
    EXPECT_EQ(reader_.peekLE<int16_t>(2).second, 0x0302);
    EXPECT_EQ(reader_.peekLE<int16_t>(4).second, -1);
    EXPECT_EQ(buffer.length(), 6);
    std::vector<std::pair<int16_t, bool>> vals = {
        {0x0100, true}, {0x0302, true}, {0xFFFF, true}, {0, false}};
    readLE<int16_t>(vals);
    EXPECT_EQ(buffer.size(), 6);
  }

  {
    std::string input;
    reader_ = StringReader(input);
    std::vector<std::pair<int16_t, bool>> vals = {{0, false}};
    EXPECT_EQ(reader_.peekLE<int16_t>().first, false);
    readLE<int16_t>(vals);
  }

  {
    std::string input(2, 0x30);
    reader_ = StringReader(input);
    std::vector<std::pair<int16_t, bool>> vals = {{0x3030, true}, {0, false}};
    EXPECT_EQ(reader_.peek<int16_t>(0).second, 0x3030);
    EXPECT_EQ(reader_.peek<int16_t>(1).first, false);
    readLE<int16_t>(vals);
  }
}

TEST_F(StringReaderTest, ReadAndPeekLEI32) {
  {
    unsigned char buf[] = {0, 1, 2, 3, 0xFF, 0xFF, 0XFF, 0XFF};
    absl::string_view buffer(reinterpret_cast<char *>(&buf), 8);
    reader_ = StringReader(buffer);
    EXPECT_EQ(reader_.peekLE<int32_t>().second, 0x03020100);
    EXPECT_EQ(reader_.peekLE<int32_t>(0).second, 0x03020100);
    EXPECT_EQ(reader_.peekLE<int32_t>(1).second, 0xFF030201);
    EXPECT_EQ(reader_.peekLE<int32_t>(2).second, 0xFFFF0302);
    EXPECT_EQ(reader_.peekLE<int32_t>(4).second, -1);
    EXPECT_EQ(buffer.length(), 8);
    std::vector<std::pair<int32_t, bool>> vals = {
        {0x03020100, true}, {0xFFFFFFFF, true}, {0, false}};
    readLE<int32_t>(vals);
    EXPECT_EQ(buffer.size(), 8);
  }

  {
    std::string input;
    reader_ = StringReader(input);
    std::vector<std::pair<int32_t, bool>> vals = {{0, false}};
    EXPECT_EQ(reader_.peekLE<int32_t>().first, false);
    readLE<int32_t>(vals);
  }

  {
    std::string input(4, 0x30);
    reader_ = StringReader(input);
    std::vector<std::pair<int32_t, bool>> vals = {{0x30303030, true},
                                                  {0, false}};
    EXPECT_EQ(reader_.peekLE<int32_t>().second, 0x30303030);
    EXPECT_EQ(reader_.peekLE<int32_t>(1).first, false);
    readLE<int32_t>(vals);
  }
}

TEST_F(StringReaderTest, ReadAndPeekLEI64) {
  {
    unsigned char buf[] = {0,    1,    2,    3,    4,    5,    6,    7,
                           0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    absl::string_view buffer(reinterpret_cast<char *>(&buf), 16);
    reader_ = StringReader(buffer);
    EXPECT_EQ(reader_.peekLE<int64_t>().second, 0x0706050403020100);
    EXPECT_EQ(reader_.peekLE<int64_t>(0).second, 0x0706050403020100);
    EXPECT_EQ(reader_.peekLE<int64_t>(1).second, 0xFF07060504030201);
    EXPECT_EQ(reader_.peekLE<int64_t>(2).second, 0xFFFF070605040302);
    EXPECT_EQ(reader_.peekLE<int64_t>(8).second, -1);
    EXPECT_EQ(buffer.length(), 16);

    std::vector<std::pair<int64_t, bool>> vals = {
        {0x0706050403020100, true}, {-1, true}, {0, false}};
    readLE<int64_t>(vals);
    EXPECT_EQ(buffer.size(), 16);
  }

  {
    std::string input;
    reader_ = StringReader(input);
    std::vector<std::pair<int64_t, bool>> vals = {{0, false}};
    EXPECT_EQ(reader_.peekLE<int64_t>(0).first, false);
    readLE<int64_t>(vals);
  }

  {
    std::string input(8, 0x30);
    reader_ = StringReader(input);
    std::vector<std::pair<int64_t, bool>> vals = {{0x3030303030303030, true},
                                                  {0, false}};
    EXPECT_EQ(reader_.peekLE<int64_t>(0).second, 0x3030303030303030);
    EXPECT_EQ(reader_.peekLE<int64_t>(1).first, false);
    readLE<int64_t>(vals);
  }
}

TEST_F(StringReaderTest, ReadAndPeekLEU16) {
  {
    unsigned char buf[] = {0, 1, 2, 3, 0xFF, 0xFF};
    absl::string_view buffer(reinterpret_cast<char *>(&buf), 6);
    reader_ = StringReader(buffer);
    EXPECT_EQ(reader_.peekLE<uint16_t>().second, 0x0100);
    EXPECT_EQ(reader_.peekLE<uint16_t>(0).second, 0x0100);
    EXPECT_EQ(reader_.peekLE<uint16_t>(1).second, 0x0201);
    EXPECT_EQ(reader_.peekLE<uint16_t>(2).second, 0x0302);
    EXPECT_EQ(reader_.peekLE<uint16_t>(4).second, 0xFFFF);
    EXPECT_EQ(buffer.length(), 6);
    std::vector<std::pair<uint16_t, bool>> vals = {
        {0x0100, true}, {0x0302, true}, {0xFFFF, true}, {0, false}};
    readLE<uint16_t>(vals);
    EXPECT_EQ(buffer.size(), 6);
  }

  {
    std::string input;
    reader_ = StringReader(input);
    std::vector<std::pair<uint16_t, bool>> vals = {{0, false}};
    readBE<uint16_t>(vals);
  }

  {
    std::string input(2, '0');
    reader_ = StringReader(input);
    std::vector<std::pair<uint16_t, bool>> vals = {{0x3030, true}, {0, false}};
    readBE<uint16_t>(vals);
  }
}

TEST_F(StringReaderTest, ReadBEU32) {
  {
    unsigned char buf[] = {0, 1, 2, 3, 0xFF, 0xFF, 0XFF, 0XFF};
    absl::string_view buffer(reinterpret_cast<char *>(&buf), 8);
    reader_ = StringReader(buffer);
    std::vector<std::pair<uint32_t, bool>> vals = {
        {0x00010203, true}, {-1, true}, {0, false}};
    readBE<uint32_t>(vals);
    EXPECT_EQ(buffer.size(), 8);
  }

  {
    std::string input;
    reader_ = StringReader(input);
    std::vector<std::pair<uint32_t, bool>> vals = {{0, false}};
    readBE<uint32_t>(vals);
  }

  {
    std::string input(4, '0');
    reader_ = StringReader(input);
    std::vector<std::pair<uint32_t, bool>> vals = {{0x30303030, true},
                                                   {0, false}};
    readBE<uint32_t>(vals);
  }
}

TEST_F(StringReaderTest, ReadBEU64) {
  {
    unsigned char buf[] = {0,    1,    2,    3,    4,    5,    6,    7,
                           0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    absl::string_view buffer(reinterpret_cast<char *>(&buf), 16);
    reader_ = StringReader(buffer);
    std::vector<std::pair<uint64_t, bool>> vals = {
        {0x0001020304050607, true}, {-1, true}, {0, false}};
    readBE<uint64_t>(vals);
    EXPECT_EQ(buffer.size(), 16);
  }

  {
    std::string input;
    reader_ = StringReader(input);
    std::vector<std::pair<uint64_t, bool>> vals = {{0, false}};
    readBE<uint64_t>(vals);
  }

  {
    std::string input(8, '0');
    reader_ = StringReader(input);
    std::vector<std::pair<uint64_t, bool>> vals = {{0x3030303030303030, true},
                                                   {0, false}};
    readBE<uint64_t>(vals);
  }
}

TEST_F(StringReaderTest, ReadString) {
  {
    std::string input("HELLO");
    reader_ = StringReader(input);
    std::string data;
    EXPECT_EQ(5, reader_.byteAvailable());
    reader_.readNBytes(Utils::allocStringBuffer(&data, 2), 1);
    EXPECT_EQ("H", data);
    EXPECT_EQ(4, reader_.byteAvailable());
    reader_.readNBytes(Utils::allocStringBuffer(&data, 3), 2);
    EXPECT_EQ("EL", data);
    EXPECT_EQ(2, reader_.byteAvailable());
  }
}

}  // namespace Hessian2
