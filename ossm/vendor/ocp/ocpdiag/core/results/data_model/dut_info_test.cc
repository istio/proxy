// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include "ocpdiag/core/results/data_model/dut_info.h"

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "ocpdiag/core/results/data_model/input_model.h"

namespace ocpdiag::results {

namespace {

TEST(DutInfoDeathTest, EmptyNameCausesDeath) {
  EXPECT_DEATH(DutInfo("", "id"), "Must specify a name");
}

TEST(DutInfoDeathTest, EmptyIdCausesDeath) {
  EXPECT_DEATH(DutInfo("name", ""), "Must specify an id");
}

TEST(DutInfoDeathTest, SingletonViolationCausesDeath) {
  DutInfo dut_info("dut", "id");
  EXPECT_DEATH(DutInfo("dut2", "id2"), "Only one DutInfo instance");
}

TEST(DutInfoTest, SequentialInitializationSucceedsWithUniqueIds) {
  std::string first_id;
  {
    DutInfo dut_info("dut", "id");
    first_id = dut_info.id();
  }
  DutInfo dut_info2("dut2", "id2");
  EXPECT_NE(first_id, dut_info2.id());
}

TEST(DutInfoTest, NameStoresCorrectly) {
  absl::string_view name = "an awesome dut";
  EXPECT_EQ(DutInfo(name, "id").name(), name);
}

TEST(DutInfoTest, IdStoresCorrectly) {
  absl::string_view id = "an awesome id";
  EXPECT_EQ(DutInfo("name", id).id(), id);
}

TEST(DutInfoTest, HardwareInfoStoresCorrectly) {
  DutInfo dut_info("dut", "id");
  HardwareInfo hw_info_1 = {.name = "hw_info_1"};
  HardwareInfo hw_info_2 = {.name = "hw_info_2"};

  EXPECT_EQ(dut_info.AddHardwareInfo(hw_info_1).id(), "0");
  EXPECT_EQ(dut_info.AddHardwareInfo(hw_info_2).id(), "1");
  EXPECT_EQ(dut_info.AddHardwareInfo(hw_info_1).id(), "2");
  EXPECT_EQ(dut_info.GetHardwareInfos()[0].hardware_info_id, "0");
  EXPECT_EQ(dut_info.GetHardwareInfos()[1].hardware_info_id, "1");
  EXPECT_EQ(dut_info.GetHardwareInfos()[2].hardware_info_id, "2");
  EXPECT_EQ(dut_info.GetHardwareInfos()[0].name, hw_info_1.name);
  EXPECT_EQ(dut_info.GetHardwareInfos()[1].name, hw_info_2.name);
  EXPECT_EQ(dut_info.GetHardwareInfos()[2].name, hw_info_1.name);
}

TEST(DutInfoDeathTest, InvalidHardwareInfoCausesDeath) {
  EXPECT_DEATH(
      []() {
        RegisteredHardwareInfo hw_info =
            DutInfo("dut", "id").AddHardwareInfo(HardwareInfo());
      }(),
      "");
}

TEST(DutInfoTest, SoftwareInfoStoresCorrectly) {
  DutInfo dut_info("dut", "id");
  SoftwareInfo sw_info_1 = {.name = "sw_info_1"};
  SoftwareInfo sw_info_2 = {.name = "sw_info_2"};

  EXPECT_EQ(dut_info.AddSoftwareInfo(sw_info_1).id(), "0");
  EXPECT_EQ(dut_info.AddSoftwareInfo(sw_info_2).id(), "1");
  EXPECT_EQ(dut_info.AddSoftwareInfo(sw_info_1).id(), "2");
  EXPECT_EQ(dut_info.GetSoftwareInfos()[0].software_info_id, "0");
  EXPECT_EQ(dut_info.GetSoftwareInfos()[1].software_info_id, "1");
  EXPECT_EQ(dut_info.GetSoftwareInfos()[2].software_info_id, "2");
  EXPECT_EQ(dut_info.GetSoftwareInfos()[0].name, sw_info_1.name);
  EXPECT_EQ(dut_info.GetSoftwareInfos()[1].name, sw_info_2.name);
  EXPECT_EQ(dut_info.GetSoftwareInfos()[2].name, sw_info_1.name);
}

TEST(DutInfoDeathTest, InvalidSoftwareInfoCausesDeath) {
  EXPECT_DEATH(
      []() {
        RegisteredSoftwareInfo sw_info =
            DutInfo("dut", "id").AddSoftwareInfo(SoftwareInfo());
      }(),
      "");
}

TEST(DutInfoTest, PlatformInfoStoresCorrectly) {
  DutInfo dut_info("dut", "id");
  PlatformInfo plat_info_1 = {.info = "plat_info_1"};
  PlatformInfo plat_info_2 = {.info = "plat_info_2"};

  dut_info.AddPlatformInfo(plat_info_2);
  dut_info.AddPlatformInfo(plat_info_1);
  dut_info.AddPlatformInfo(plat_info_2);
  EXPECT_EQ(dut_info.GetPlatformInfos()[0].info, plat_info_2.info);
  EXPECT_EQ(dut_info.GetPlatformInfos()[1].info, plat_info_1.info);
  EXPECT_EQ(dut_info.GetPlatformInfos()[2].info, plat_info_2.info);
}

TEST(DutInfoDeathTest, InvalidPlatformInfoCausesDeath) {
  EXPECT_DEATH(DutInfo("dut", "id").AddPlatformInfo(PlatformInfo()), "");
}

TEST(DutInfoTest, MetadataJsonStoresCorrectly) {
  DutInfo dut_info("dut", "id");
  absl::string_view json = "{\"some\":\"JSON\"}";
  dut_info.SetMetadataJson(json);
  EXPECT_EQ(dut_info.GetMetadataJson(), json);
}

}  // namespace

}  // namespace ocpdiag::results
