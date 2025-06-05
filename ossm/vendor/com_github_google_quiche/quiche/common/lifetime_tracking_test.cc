#include "quiche/common/lifetime_tracking.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "quiche/common/platform/api/quiche_logging.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace quiche {
namespace test {

struct ComposedTrackable {
  LifetimeTrackable trackable;
};

struct InheritedTrackable : LifetimeTrackable {};

enum class TrackableType {
  kComposed,
  kInherited,
};

// Used by ::testing::PrintToStringParamName().
std::string PrintToString(const TrackableType& type) {
  switch (type) {
    case TrackableType::kComposed:
      return "Composed";
    case TrackableType::kInherited:
      return "Inherited";
    default:
      QUICHE_LOG(FATAL) << "Unknown TrackableType: " << static_cast<int>(type);
  }
}

class LifetimeTrackingTest : public QuicheTestWithParam<TrackableType> {
 protected:
  LifetimeTrackingTest() {
    if (GetParam() == TrackableType::kComposed) {
      composed_trackable_ = std::make_unique<ComposedTrackable>();
    } else {
      inherited_trackable_ = std::make_unique<InheritedTrackable>();
    }
  }

  // Returns the trackable object. Must be called before FreeTrackable.
  LifetimeTrackable& GetTrackable() {
    if (composed_trackable_ != nullptr) {
      return composed_trackable_->trackable;
    } else {
      return *inherited_trackable_;
    }
  }

  // Returns a trackable.info_.
  const std::shared_ptr<LifetimeInfo>& GetLifetimeInfoFromTrackable(
      LifetimeTrackable& trackable) {
    return trackable.info_;
  }

  const std::shared_ptr<LifetimeInfo>& GetLifetimeInfoFromTrackable() {
    return GetLifetimeInfoFromTrackable(GetTrackable());
  }

  void FreeTrackable() {
    composed_trackable_ = nullptr;
    inherited_trackable_ = nullptr;
  }

  std::unique_ptr<ComposedTrackable> composed_trackable_;
  std::unique_ptr<InheritedTrackable> inherited_trackable_;
};

TEST_P(LifetimeTrackingTest, TrackableButNeverTracked) {
  EXPECT_EQ(GetLifetimeInfoFromTrackable(), nullptr);
}

TEST_P(LifetimeTrackingTest, SingleTrackerQueryLiveness) {
  LifetimeTracker tracker = GetTrackable().NewTracker();
  EXPECT_FALSE(tracker.IsTrackedObjectDead());
  EXPECT_THAT(absl::StrCat(tracker),
              testing::HasSubstr("Tracked object is alive"));
  FreeTrackable();
  EXPECT_TRUE(tracker.IsTrackedObjectDead());
  EXPECT_THAT(absl::StrCat(tracker),
              testing::HasSubstr("Tracked object has died"));
}

TEST_P(LifetimeTrackingTest, MultiTrackersQueryLiveness) {
  LifetimeTracker tracker1 = GetTrackable().NewTracker();
  LifetimeTracker tracker2 = GetTrackable().NewTracker();
  LifetimeTracker tracker3 = tracker2;
  LifetimeTracker tracker4 = std::move(tracker3);
  LifetimeTracker tracker5(std::move(tracker4));
  LifetimeTrackable another_trackable;
  LifetimeTracker tracker6 = another_trackable.NewTracker();
  LifetimeTracker tracker7 = another_trackable.NewTracker();
  tracker6 = tracker2;
  tracker7 = std::move(tracker2);
  EXPECT_FALSE(tracker1.IsTrackedObjectDead());
  EXPECT_FALSE(
      tracker2.IsTrackedObjectDead());  // NOLINT(bugprone-use-after-move)
  EXPECT_FALSE(
      tracker3.IsTrackedObjectDead());  // NOLINT(bugprone-use-after-move)
  EXPECT_FALSE(
      tracker4.IsTrackedObjectDead());  // NOLINT(bugprone-use-after-move)
  EXPECT_FALSE(tracker5.IsTrackedObjectDead());
  EXPECT_FALSE(tracker6.IsTrackedObjectDead());
  EXPECT_FALSE(tracker7.IsTrackedObjectDead());
  FreeTrackable();
  EXPECT_TRUE(tracker1.IsTrackedObjectDead());
  EXPECT_TRUE(
      tracker2.IsTrackedObjectDead());  // NOLINT(bugprone-use-after-move)
  EXPECT_TRUE(
      tracker3.IsTrackedObjectDead());  // NOLINT(bugprone-use-after-move)
  EXPECT_TRUE(
      tracker4.IsTrackedObjectDead());  // NOLINT(bugprone-use-after-move)
  EXPECT_TRUE(tracker5.IsTrackedObjectDead());
  EXPECT_TRUE(tracker6.IsTrackedObjectDead());
  EXPECT_TRUE(tracker7.IsTrackedObjectDead());
}

TEST_P(LifetimeTrackingTest, SingleTrackerAnnotations) {
  LifetimeTracker tracker = GetTrackable().NewTracker();
  GetTrackable().Annotate("for what shall it profit a man");
  GetTrackable().Annotate("if he shall gain a stack trace");
  GetTrackable().Annotate("but lose all of the context");
  FreeTrackable();
  EXPECT_TRUE(tracker.IsTrackedObjectDead());
  const std::string serialized = absl::StrCat(tracker);
  EXPECT_THAT(serialized, testing::HasSubstr("Tracked object has died"));
  EXPECT_THAT(serialized, testing::HasSubstr("for what shall"));
  EXPECT_THAT(serialized, testing::HasSubstr("gain a stack trace"));
  EXPECT_THAT(serialized, testing::HasSubstr("lose all of the context"));
}

TEST_P(LifetimeTrackingTest, CopyTrackableIsNoop) {
  LifetimeTracker tracker = GetTrackable().NewTracker();
  const LifetimeInfo* info = GetLifetimeInfoFromTrackable().get();
  EXPECT_NE(info, nullptr);
  LifetimeTrackable trackable2(GetTrackable());
  EXPECT_EQ(GetLifetimeInfoFromTrackable(trackable2), nullptr);

  LifetimeTrackable trackable3;
  trackable3 = GetTrackable();
  EXPECT_EQ(GetLifetimeInfoFromTrackable(trackable3), nullptr);

  EXPECT_EQ(GetLifetimeInfoFromTrackable().get(), info);
}

TEST_P(LifetimeTrackingTest, MoveTrackableIsNoop) {
  LifetimeTracker tracker = GetTrackable().NewTracker();
  const LifetimeInfo* info = GetLifetimeInfoFromTrackable().get();
  EXPECT_NE(info, nullptr);
  LifetimeTrackable trackable2(std::move(GetTrackable()));
  EXPECT_EQ(GetLifetimeInfoFromTrackable(trackable2), nullptr);

  LifetimeTrackable trackable3;
  trackable3 = std::move(GetTrackable());
  EXPECT_EQ(GetLifetimeInfoFromTrackable(trackable3), nullptr);

  EXPECT_EQ(GetLifetimeInfoFromTrackable().get(), info);
}

TEST_P(LifetimeTrackingTest, ObjectDiedDueToVectorRealloc) {
  if (GetParam() == TrackableType::kComposed) {
    return;
  }

  std::vector<InheritedTrackable> trackables;

  // Append 1 element to the vector and keep track of its life.
  InheritedTrackable& trackable = trackables.emplace_back();
  LifetimeTracker tracker = trackable.NewTracker();
  EXPECT_FALSE(tracker.IsTrackedObjectDead());

  // Append 1000 more elements to the vector, |trackable| should be destroyed by
  // vector realloc.
  for (int i = 0; i < 1000; ++i) {
    trackables.emplace_back();
  }

  // Accessing |trackable| is a use-after-free.
  EXPECT_TRUE(tracker.IsTrackedObjectDead());
}

INSTANTIATE_TEST_SUITE_P(Tests, LifetimeTrackingTest,
                         testing::Values(TrackableType::kComposed,
                                         TrackableType::kInherited),
                         testing::PrintToStringParamName());

}  // namespace test
}  // namespace quiche
