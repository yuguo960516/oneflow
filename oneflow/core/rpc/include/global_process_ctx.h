/*
Copyright 2020 The OneFlow Authors. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#ifndef ONEFLOW_CORE_RPC_INCLUDE_GLOBAL_PROCESS_CTX_
#define ONEFLOW_CORE_RPC_INCLUDE_GLOBAL_PROCESS_CTX_

#include <string>
#include "oneflow/core/common/util.h"
#include "oneflow/core/control/ctrl_bootstrap.pb.h"

namespace oneflow {

struct GlobalProcessCtx {
  static void GetCurrentMachineIdAndDeviceId(int64_t* machine_id, int64_t* device_id);
  static int64_t Rank();
  static int64_t NodeSize();
  static int64_t ThisNodeId();
  static int64_t NodeId4Rank(int64_t rank);
  static HashMap<int64_t, int64_t> NodeId2RankOffset();
  static int64_t NumOfProcessOnNode();
  static const NumProcessDistribution& NumProcessDistributionInCluster();
  static bool IsThisProcessMaster();
  static size_t WorldSize();
  static std::string LogDirEntry();
};

}  // namespace oneflow

#endif  // ONEFLOW_CORE_RPC_INCLUDE_GLOBAL_PROCESS_CTX_
