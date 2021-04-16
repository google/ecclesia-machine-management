/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ecclesia/magent/sysmodel/x86/sysmodel.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "ecclesia/lib/io/pci/discovery.h"
#include "ecclesia/lib/io/pci/location.h"
#include "ecclesia/magent/sysmodel/x86/fru.h"
#include "ecclesia/magent/sysmodel/x86/nvme.h"
#include "ecclesia/magent/sysmodel/x86/nvme_mock.h"
#include "ecclesia/magent/sysmodel/x86/thermal.h"

namespace ecclesia {
namespace {

using ::testing::UnorderedElementsAre;

TEST(SystemModelTest, EmptyParamsNoBreak) {
  SysmodelParams params = {
      .field_translator = nullptr,
      .smbios_entry_point_path = "",
      .smbios_tables_path = "",
      .mced_socket_path = "",
      .sysfs_mem_file_path = "",
      .fru_factories = absl::Span<SysmodelFruReaderFactory>(),
      .dimm_thermal_params = absl::Span<PciSensorParams>(),
      .cpu_margin_params = absl::Span<CpuMarginSensorParams>(),
      .nvme_discover_getter = nullptr};
  SystemModel sysmodel(std::move(params));

  EXPECT_EQ(sysmodel.NumCpus(), 0);
  EXPECT_EQ(sysmodel.NumDimms(), 0);
  EXPECT_EQ(sysmodel.NumDimmThermalSensors(), 0);
  EXPECT_EQ(sysmodel.NumCpuMarginSensors(), 0);
  EXPECT_EQ(sysmodel.NumFruReaders(), 0);
  EXPECT_EQ(sysmodel.NumNvmePlugins(), 0);
}

TEST(SystemModelTest, DiscoverNvmePluginsFailure) {
  auto nvme_discover = std::make_unique<MockNvmeDiscover>();
  EXPECT_CALL(*nvme_discover, GetAllNvmePlugins()).WillOnce([&](auto...) {
    return absl::InternalError("Some error for GetAllNvmePlugins");
  });

  SysmodelParams params = {
      .field_translator = nullptr,
      .smbios_entry_point_path = "",
      .smbios_tables_path = "",
      .mced_socket_path = "",
      .sysfs_mem_file_path = "",
      .fru_factories = absl::Span<SysmodelFruReaderFactory>(),
      .dimm_thermal_params = absl::Span<PciSensorParams>(),
      .cpu_margin_params = absl::Span<CpuMarginSensorParams>(),
      .nvme_discover_getter = [&](PciTopologyInterface *) {
        return std::move(nvme_discover);
      }};
  SystemModel sysmodel(std::move(params));

  // No NVMe plugin is created. The error message will be printed to ErrorLog().
  EXPECT_EQ(sysmodel.NumNvmePlugins(), 0);
}

TEST(SystemModelTest, GetCorrectNvmePlugin) {
  // Set up mock for MockNvmeDiscover::GetAllNvmePlugins
  NvmeLocation nvme_location0{PciDbdfLocation::Make<0, 0xae, 3, 4>(), "U2_1"};
  NvmeLocation nvme_location1{PciDbdfLocation::Make<0, 0xd8, 9, 0>(), "U2_6"};
  std::vector<NvmePlugin> nvme_plugins;
  nvme_plugins.push_back(NvmePlugin{
      nvme_location0, std::make_unique<NvmeInterface>(nullptr, "nvme1")});
  nvme_plugins.push_back(NvmePlugin{
      nvme_location1, std::make_unique<NvmeInterface>(nullptr, "nvme6")});

  auto nvme_discover = std::make_unique<MockNvmeDiscover>();
  EXPECT_CALL(*nvme_discover, GetAllNvmePlugins()).WillOnce([&](auto...) {
    return std::move(nvme_plugins);
  });

  SysmodelParams params = {
      .field_translator = nullptr,
      .smbios_entry_point_path = "",
      .smbios_tables_path = "",
      .mced_socket_path = "",
      .sysfs_mem_file_path = "",
      .fru_factories = absl::Span<SysmodelFruReaderFactory>(),
      .dimm_thermal_params = absl::Span<PciSensorParams>(),
      .cpu_margin_params = absl::Span<CpuMarginSensorParams>(),
      .nvme_discover_getter = [&](PciTopologyInterface *) {
        return std::move(nvme_discover);
      }};
  SystemModel sysmodel(std::move(params));

  // Verify getting correct NVMe plugins from sysmodel.
  ASSERT_EQ(sysmodel.NumNvmePlugins(), 2);
  EXPECT_THAT(sysmodel.GetNvmePhysLocations(),
              UnorderedElementsAre("U2_1", "U2_6"));
  std::vector<NvmeLocation> actual_nvme_locations;
  std::vector<std::string> actual_nvme_names;
  auto maybe_nvme_plugin = sysmodel.GetNvmeByPhysLocation("U2_1");
  ASSERT_TRUE(maybe_nvme_plugin.has_value());
  EXPECT_EQ(maybe_nvme_plugin->location, nvme_location0);
  EXPECT_EQ(maybe_nvme_plugin->access_intf->GetKernelName(), "nvme1");

  maybe_nvme_plugin = sysmodel.GetNvmeByPhysLocation("U2_6");
  ASSERT_TRUE(maybe_nvme_plugin.has_value());
  EXPECT_EQ(maybe_nvme_plugin->location, nvme_location1);
  EXPECT_EQ(maybe_nvme_plugin->access_intf->GetKernelName(), "nvme6");

  // Find a non-exist NVMe
  maybe_nvme_plugin = sysmodel.GetNvmeByPhysLocation("U2_3");
  EXPECT_FALSE(maybe_nvme_plugin.has_value());
}

TEST(SystemModelTest, GetSystemUptime) {
  SysmodelParams params = {
      .field_translator = nullptr,
      .smbios_entry_point_path = "",
      .smbios_tables_path = "",
      .mced_socket_path = "",
      .sysfs_mem_file_path = "",
      .fru_factories = absl::Span<SysmodelFruReaderFactory>(),
      .dimm_thermal_params = absl::Span<PciSensorParams>(),
      .cpu_margin_params = absl::Span<CpuMarginSensorParams>(),
      .nvme_discover_getter = nullptr};
  SystemModel sysmodel(std::move(params));

  auto system_uptime = sysmodel.GetSystemUptimeSeconds();
  EXPECT_TRUE(system_uptime.ok());
  EXPECT_GE(*system_uptime, 0);
}

TEST(SystemModelTest, GetSystemTotalMemory) {
  SysmodelParams params = {
      .field_translator = nullptr,
      .smbios_entry_point_path = "",
      .smbios_tables_path = "",
      .mced_socket_path = "",
      .sysfs_mem_file_path = "",
      .fru_factories = absl::Span<SysmodelFruReaderFactory>(),
      .dimm_thermal_params = absl::Span<PciSensorParams>(),
      .cpu_margin_params = absl::Span<CpuMarginSensorParams>(),
      .nvme_discover_getter = nullptr};
  SystemModel sysmodel(std::move(params));

  auto total_memory_size = sysmodel.GetSystemTotalMemoryBytes();
  EXPECT_TRUE(total_memory_size.ok());
  EXPECT_GE(*total_memory_size, 0);
}

}  // namespace
}  // namespace ecclesia
