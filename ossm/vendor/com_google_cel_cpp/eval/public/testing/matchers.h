#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_TESTING_MATCHERS_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_TESTING_MATCHERS_H_

#include <cstdint>
#include <ostream>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "eval/public/cel_value.h"
#include "internal/testing.h"
#include "google/protobuf/message.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// GTest Printer
void PrintTo(const CelValue& value, std::ostream* os);

namespace test {

// readability alias
using CelValueMatcher = testing::Matcher<CelValue>;

// Tests equality to CelValue v using the set_util implementation.
CelValueMatcher EqualsCelValue(const CelValue& v);

// Matches CelValues of type null.
CelValueMatcher IsCelNull();

// Matches CelValues of type bool whose held value matches |m|.
CelValueMatcher IsCelBool(testing::Matcher<bool> m);

// Matches CelValues of type int64 whose held value matches |m|.
CelValueMatcher IsCelInt64(testing::Matcher<int64_t> m);

// Matches CelValues of type uint64_t whose held value matches |m|.
CelValueMatcher IsCelUint64(testing::Matcher<uint64_t> m);

// Matches CelValues of type double whose held value matches |m|.
CelValueMatcher IsCelDouble(testing::Matcher<double> m);

// Matches CelValues of type string whose held value matches |m|.
CelValueMatcher IsCelString(testing::Matcher<absl::string_view> m);

// Matches CelValues of type bytes whose held value matches |m|.
CelValueMatcher IsCelBytes(testing::Matcher<absl::string_view> m);

// Matches CelValues of type message whose held value matches |m|.
CelValueMatcher IsCelMessage(testing::Matcher<const google::protobuf::Message*> m);

// Matches CelValues of type duration whose held value matches |m|.
CelValueMatcher IsCelDuration(testing::Matcher<absl::Duration> m);

// Matches CelValues of type timestamp whose held value matches |m|.
CelValueMatcher IsCelTimestamp(testing::Matcher<absl::Time> m);

// Matches CelValues of type error whose held value matches |m|.
// The matcher |m| is wrapped to allow using the testing::status::... matchers.
CelValueMatcher IsCelError(testing::Matcher<absl::Status> m);

// A matcher that wraps a Container matcher so that container matchers can be
// used for matching CelList.
//
// This matcher can be avoided if CelList supported the iterators needed by the
// standard container matchers but given that it is an interface it is a much
// larger project.
//
// TODO(issues/73): Re-use CelValueMatcherImpl. There are template details
// that need to be worked out specifically on how CelValueMatcherImpl can accept
// a generic matcher for CelList instead of testing::Matcher<CelList>.
template <typename ContainerMatcher>
class CelListMatcher : public testing::MatcherInterface<const CelValue&> {
 public:
  explicit CelListMatcher(ContainerMatcher m) : container_matcher_(m) {}

  bool MatchAndExplain(const CelValue& v,
                       testing::MatchResultListener* listener) const override {
    const CelList* cel_list;
    if (!v.GetValue(&cel_list) || cel_list == nullptr) return false;

    std::vector<CelValue> cel_vector;
    cel_vector.reserve(cel_list->size());
    for (int i = 0; i < cel_list->size(); ++i) {
      cel_vector.push_back((*cel_list)[i]);
    }
    return container_matcher_.Matches(cel_vector);
  }

  void DescribeTo(std::ostream* os) const override {
    CelValue::Type type =
        static_cast<CelValue::Type>(CelValue::IndexOf<const CelList*>::value);
    *os << absl::StrCat("type is ", CelValue::TypeName(type), " and ");
    container_matcher_.DescribeTo(os);
  }

 private:
  const testing::Matcher<std::vector<CelValue>> container_matcher_;
};

template <typename ContainerMatcher>
CelValueMatcher IsCelList(ContainerMatcher m) {
  return CelValueMatcher(new CelListMatcher(m));
}
// TODO(issues/73): add helpers for working with maps and unknown sets.

}  // namespace test
}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_TESTING_MATCHERS_H_
