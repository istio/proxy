#include <fstream>

#include "absl/strings/str_cat.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "jwt_verify_lib/jwks.h"
#include "jwt_verify_lib/jwt.h"
#include "jwt_verify_lib/verify.h"
#include "test/fuzz/jwt_verify_lib_fuzz_input.pb.h"

namespace google {
namespace jwt_verify {
namespace {

const absl::string_view kRunfilesDir = std::getenv("TEST_SRCDIR");
const absl::string_view kWorkingDir = std::getenv("TEST_WORKSPACE");
constexpr absl::string_view kDataDir =
    "test/fuzz/corpus/jwt_verify_lib_fuzz_test";

std::string ReadTestBaseline(const std::string& input_file_name) {
  // Must reference testdata with an absolute path.
  std::string file_name = absl::StrCat(kRunfilesDir, "/", kWorkingDir, "/",
                                       kDataDir, "/", input_file_name);

  std::string contents;
  std::ifstream input_file;
  input_file.open(file_name, std::ifstream::in | std::ifstream::binary);
  EXPECT_TRUE(input_file.is_open()) << file_name;
  input_file.seekg(0, std::ios::end);
  contents.reserve(input_file.tellg());
  input_file.seekg(0, std::ios::beg);
  contents.assign((std::istreambuf_iterator<char>(input_file)),
                  (std::istreambuf_iterator<char>()));

  return contents;
}

// Each corpus file has "jwt" and "jwks". If they are valid and
// "jwks" can be used to verify "jwt", they will help fuzz engine
// to be more efficient.
// This test verifies the following corpus files satisfy above conditions.
TEST(JwksParseTest, FuzzTestJwksCorpusFile) {
  std::vector<std::string> files = {"jwks_ec.txt",   "jwks_rsa.txt",
                                    "jwks_hmac.txt", "jwks_okp.txt",
                                    "jwks_x509.txt", "jwks_pem.txt"};
  for (const auto& file : files) {
    const std::string txt = ReadTestBaseline(file);
    FuzzInput input;
    EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(txt, &input))
        << "failed to parse corpus file: " << file;

    Jwt jwt;
    EXPECT_EQ(jwt.parseFromString(input.jwt()), Status::Ok)
        << "failed to parse jwt in corpus file: " << file;

    const Jwks::Type jwks_type =
        (file == "jwks_pem.txt") ? Jwks::PEM : Jwks::JWKS;
    auto jwks = Jwks::createFrom(input.jwks(), jwks_type);
    EXPECT_EQ(jwks->getStatus(), Status::Ok)
        << "failed to parse jwks in corpusfile: " << file;

    // Use to timestamp "1", not to verify expiration.
    EXPECT_EQ(getStatusString(verifyJwt(jwt, *jwks, 1)),
              getStatusString(Status::Ok))
        << "failed to verify in corpus file: " << file;
  }
}

}  // namespace
}  // namespace jwt_verify
}  // namespace google
