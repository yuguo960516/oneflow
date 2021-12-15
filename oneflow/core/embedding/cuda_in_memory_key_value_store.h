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
#ifndef ONEFLOW_EMBEDDING_CUDA_IN_MEMORY_KEY_VALUE_STORE_H_
#define ONEFLOW_EMBEDDING_CUDA_IN_MEMORY_KEY_VALUE_STORE_H_

#include "oneflow/core/embedding/key_value_store.h"
#include "oneflow/core/common/data_type.h"

namespace oneflow {

namespace embedding {

struct CudaInMemoryKeyValueStoreOptions {
  enum class EncodingType {
    kPlain,
    kOrdinal,
  };
  size_t num_shards = 0;
  size_t value_length = 0;
  DataType key_type = DataType::kInvalidDataType;
  DataType value_type = DataType::kInvalidDataType;
  size_t num_keys = 0;
  size_t num_device_keys = 0;
  EncodingType encoding_type = EncodingType::kPlain;
};

std::unique_ptr<KeyValueStore> NewCudaInMemoryKeyValueStore(
    const CudaInMemoryKeyValueStoreOptions& options);

}  // namespace embedding

}  // namespace oneflow

#endif  // ONEFLOW_EMBEDDING_CUDA_IN_MEMORY_KEY_VALUE_STORE_H_
