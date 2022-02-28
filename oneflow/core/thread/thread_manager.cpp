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
#include "oneflow/core/thread/thread_manager.h"
#include "oneflow/core/job/resource_desc.h"
#include "oneflow/core/job/global_for.h"
#include "oneflow/core/control/global_process_ctx.h"

namespace oneflow {

ThreadMgr::~ThreadMgr() {
  for (auto& thread_pair : threads_) {
    CHECK(thread_pair.second) << " Runtime Error! thread id " << thread_pair.first
                              << " not delete with graph, and it's empty is "
                              << thread_pair.second->Empty();
  }
}

Thread* ThreadMgr::GetThrd(int64_t thrd_id) {
  auto iter = threads_.find(thrd_id);
  CHECK(iter != threads_.end()) << "thread " << thrd_id << " not found";
  return iter->second.get();
}

void ThreadMgr::AddThreads(const std::vector<int64_t>& thread_ids) {
  const int64_t this_rank = GlobalProcessCtx::Rank();
  for (int64_t thrd_id : thread_ids) {
    if (threads_.find(thrd_id) != threads_.end()) { continue; }
    StreamId stream_id = DecodeStreamIdFromInt64(thrd_id);
    if (stream_id.rank() != this_rank) { continue; }
    Thread* thread = new Thread(stream_id);
    CHECK_NOTNULL(thread);
    threads_[thrd_id].reset(thread);
  }
}

void ThreadMgr::TryDeleteThreads(const std::vector<int64_t>& thread_ids) {
  for (int64_t thrd_id : thread_ids) {
    auto it = threads_.find(thrd_id);
    if (it == threads_.end()) { continue; }
    auto& thread = it->second;
    if (thread->Empty()) {
      // NOTE(chengcheng):  Only delete thread when it is empty.
      ActorMsg msg = ActorMsg::BuildCommandMsg(-1, ActorCmd::kStopThread);
      thread->GetMsgChannelPtr()->Send(msg);
      thread.reset();
      LOG(INFO) << " actor thread " << thrd_id << " finish.";
    } else {
      LOG(INFO) << " actor thread " << thrd_id << " not delete because it is not empty.";
    }
  }
}

void SingleThreadLoop(size_t num, std::function<void(size_t i)> Callback) {
  FOR_RANGE(size_t, i, 0, num) { Callback(i); }
}

}  // namespace oneflow
