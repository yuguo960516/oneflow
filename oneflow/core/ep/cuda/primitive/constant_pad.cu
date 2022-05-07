// /*
// Copyright 2020 The OneFlow Authors. All rights reserved.

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// */
// #include "oneflow/core/ep/include/primitive/constant_pad.h"
// #include "oneflow/core/ep/common/primitive/constant_pad.h"
// #include "oneflow/core/ep/cuda/primitive/type_seq.h"
// #include "oneflow/core/ep/cuda/cuda_stream.h"
// #include <cuda_runtime.h>

// namespace oneflow {

// namespace ep {

// namespace primitive {

// namespace {

// constexpr int32_t kMaxNumDims = 8;
// constexpr int32_t Min(int32_t a, int32_t b) { return a < b ? a : b; }
// constexpr int32_t kMaxPackBytes = 128 / 8;

// template<typename T>
// constexpr int32_t GetMaxPackSize() {
//   return Min(kMaxPackBytes / sizeof(T), 8);
// }

// template<typename T, int pack_size>
// struct GetPackType {
//   using type = typename std::aligned_storage<pack_size * sizeof(T), pack_size * sizeof(T)>::type;
// };

// template<typename T, int pack_size>
// using PackType = typename GetPackType<T, pack_size>::type;

// template<typename T, size_t pack_size>
// union Pack {
//   static_assert(sizeof(PackType<T, pack_size>) == sizeof(T) * pack_size, "");
//   explicit __host__ __device__ Pack(T value) {
// #pragma unroll
//     for (int i = 0; i < pack_size; i++) { elem[i] = value; }
//   }
//   T elem[pack_size];
//   PackType<T, pack_size> storage;
// };


// // -> typename T, packsize -> typename PackType
// template<size_t num_dims, typename IndexType, typename T, int pack_size>
// __global__ void ConstantPadKernel(ConstantPadParams<num_dims, IndexType> params, T pad_value) {
//   IndexType global_thread_id = blockIdx.x * blockDim.x + threadIdx.x;
//   using LoadStoreType = PackType<T, pack_size>;
//   const LoadStoreType* src = reinterpret_cast<const LoadStoreType*>(params.src);
//   LoadStoreType* dst = reinterpret_cast<LoadStoreType*>(params.dst);
//   IndexType src_index[num_dims];
//   IndexType dst_index[num_dims];
//   // todo: Use cuda1d kernel loop
//   for (IndexType linear_index = global_thread_id; linear_index < params.elem_cnt;
//        linear_index += gridDim.x * blockDim.x) {
//     // 做下实验
//     // kMaxDims 声明数组
//     // num_dims 动态
//     /*
//     for(i = 0; i < k<MadDims; i++){
//       if i >= num_dims: 
//         break 
//       else: 
//     }
//     */
//     params.dst_index_helper.OffsetToNdIndex(linear_index, dst_index);
//     bool if_pad = false;
// #pragma unroll
//     for (int i = 0; i < num_dims; i++) {
//       if (dst_index[i] >= params.padding_before[i]
//           && dst_index[i] < params.out_size[i] - params.padding_after[i]) {
//         src_index[i] = dst_index[i] - params.padding_before[i];
//       } else {
//         if_pad = true;
//         break;
//       }
//     }
//     if (!if_pad) {
//       const IndexType src_offset = params.src_index_helper.NdIndexToOffset(src_index);
//       dst[linear_index] = src[src_offset];
//     } else {
//       Pack<T, pack_size> packed_pad_val(pad_value);
//       dst[linear_index] = packed_pad_val.storage;
//     }
//   }
// }

// template<typename T>
// T GetValue(Scalar value) {
//   return value.Value<T>();
// }

// template<>
// half GetValue<half>(Scalar value) {
//   return static_cast<half>(GetValue<float>(value));
// }

// #if CUDA_VERSION >= 11000

// template<>
// nv_bfloat16 GetValue<nv_bfloat16>(Scalar value) {
//   return static_cast<nv_bfloat16>(GetValue<float>(value));
// }

// #endif  // CUDA_VERSION >= 11000

// template<size_t max_pack_size>
// size_t GetLaunchPackSize(size_t elem_size, size_t num_dims, void* dst, const int64_t* dst_dims,
//                          const void* src, const int64_t* src_dims, const int64_t* padding_before,
//                          const int64_t* padding_after) {
//   static_assert(max_pack_size > 0 && (max_pack_size & (max_pack_size - 1)) == 0, "");
//   CHECK_GT(elem_size, 0);
//   CHECK_EQ((elem_size & (elem_size - 1)), 0);
//   CHECK_EQ(max_pack_size % elem_size, 0);
//   const int64_t last_dst_dim_size = dst_dims[num_dims - 1];
//   const int64_t last_src_dim_size = src_dims[num_dims - 1];
//   const int64_t last_padding_before_size = padding_before[num_dims - 1];
//   const int64_t last_padding_after_size = padding_after[num_dims - 1];
//   auto src_ptr = reinterpret_cast<std::uintptr_t>(src);
//   auto dst_ptr = reinterpret_cast<std::uintptr_t>(dst);
//   // todo: think padding is negative val. 
//   for (size_t size = max_pack_size; size > 1; size /= 2) {
//     if (last_dst_dim_size % size == 0 && last_src_dim_size % size == 0
//         && last_padding_before_size % size == 0 && last_padding_after_size % size == 0
//         && src_ptr % size == 0 && dst_ptr % size == 0) {
//       return size;
//     }
//   }
//   return 1;
// }

// template<size_t num_dims, typename IndexType, typename T, size_t pack_size>
// void LaunchKernel(Stream* stream, ConstantPadParams<num_dims, IndexType> params, T pad_val) {
//   cudaStream_t cuda_stream = stream->As<CudaStream>()->cuda_stream();
//   ConstantPadKernel<num_dims, IndexType, T, pack_size>
//       <<<BlocksNum4ThreadsNum(params.elem_cnt / pack_size), kCudaThreadsNumPerBlock, 0,
//          cuda_stream>>>(params, pad_val);
// }

// template<size_t num_dims, typename IndexType, typename T, size_t pack_size>
// void LaunchKernel(Stream* stream, void* dst, const int64_t* dst_dims, const void* src,
//                   const int64_t* src_dims, const int64_t* padding_before,
//                   const int64_t* padding_after, T pad_val) {
//   ConstantPadParams<num_dims, IndexType> params;
//   params.dst_index_helper = NdIndexOffsetHelper<IndexType, num_dims>(dst_dims);
//   params.src_index_helper = NdIndexOffsetHelper<IndexType, num_dims>(src_dims);
//   params.dst = dst;
//   params.src = src;
//   size_t elem_cnt = 1;
//   for (int i = 0; i < num_dims; i++) {
//     params.padding_before[i] = padding_before[i];
//     params.padding_after[i] = padding_after[i];
//     params.out_size[i] = dst_dims[i];
//     elem_cnt *= params.out_size[i];
//   }
//   params.elem_cnt = elem_cnt;
//   LaunchKernel<num_dims, IndexType, T, pack_size>(stream, params, pad_val);
// }

// template<size_t num_dims, typename T, size_t pack_size>
// void DispatchIndexType(Stream* stream, void* dst, const int64_t* dst_dims, const void* src,
//                        const int64_t* src_dims, const int64_t* padding_before,
//                        const int64_t* padding_after, T pad_val) {
//   size_t elem_cnt = 1;
//   for (size_t i = 0; i < num_dims; ++i) { elem_cnt *= dst_dims[i]; }
//   if (elem_cnt < GetMaxVal<int32_t>()) {
//     LaunchKernel<num_dims, int32_t, T, pack_size>(stream, dst, dst_dims, src, src_dims,
//                                                   padding_before, padding_after, pad_val);
//   } else {
//     LaunchKernel<num_dims, int64_t, T, pack_size>(stream, dst, dst_dims, src, src_dims,
//                                                   padding_before, padding_after, pad_val);
//   }
// }

// void SimplifyPadDims(size_t num_dims, const int64_t* dst_dims, const int64_t* src_dims,
//                      const int64_t* padding_before, const int64_t* padding_after,
//                      size_t* simplified_num_dims, int64_t* simplified_dst_dims,
//                      int64_t* simplified_src_dims, int64_t* simplified_padding_before,
//                      int64_t* simplified_padding_after) {
//   CHECK_NE(num_dims, 0);
//   size_t valid_num_dims = 0;
//   FOR_RANGE(size_t, i, 0, num_dims) {
//     if ((i != 0) && (dst_dims[i] == src_dims[i])) {
//       simplified_dst_dims[valid_num_dims - 1] *= dst_dims[i];
//       simplified_src_dims[valid_num_dims - 1] *= src_dims[i];
//       simplified_padding_before[valid_num_dims - 1] *= src_dims[i];
//       simplified_padding_after[valid_num_dims - 1] *= src_dims[i];
//     } else {
//       simplified_dst_dims[valid_num_dims] = dst_dims[i];
//       simplified_src_dims[valid_num_dims] = src_dims[i];
//       simplified_padding_before[valid_num_dims] = padding_before[i];
//       simplified_padding_after[valid_num_dims] = padding_after[i];
//       valid_num_dims += 1;
//     }
//   }
//   *simplified_num_dims = valid_num_dims;
// }

// template<size_t num_dims, typename T>
// void DispatchPackSize(Stream* stream, void* dst, int64_t* dst_dims, const void* src,
//                       int64_t* src_dims, int64_t* padding_before, int64_t* padding_after,
//                       T pad_val) {
//   constexpr int32_t max_packsize = GetMaxPackSize<T>();
//   size_t launch_pack_size = GetLaunchPackSize<max_packsize>(
//       sizeof(T), num_dims, dst, dst_dims, src, src_dims, padding_before, padding_after);

//   printf("simplified num dims is: %ld \n", num_dims);
//   printf("Launch pack size is: %ld \n", launch_pack_size);
//   for (int i = 0; i < num_dims; i++) {
//     printf("simplified_dst_dims %d is: %ld \n", i, dst_dims[i]);
//   }
//   for (int i = 0; i < num_dims; i++) {
//     printf("simplified_src_dims %d is: %ld \n", i, src_dims[i]);
//   }
//   for (int i = 0; i < num_dims; i++) {
//     printf("padding before %d is: %ld \n", i, padding_before[i]);
//   }
//   for (int i = 0; i < num_dims; i++) { printf("padding after %d is: %ld \n", i, padding_after[i]); }

//   dst_dims[num_dims - 1] /= launch_pack_size;
//   src_dims[num_dims - 1] /= launch_pack_size;
//   padding_before[num_dims - 1] /= launch_pack_size;
//   padding_after[num_dims - 1] /= launch_pack_size;

//   void (*func)(Stream* /*stream*/, void* /*dst*/, const int64_t* /*dst_dims*/, const void* /*src*/,
//                const int64_t* /*src_dims*/, const int64_t* /*padding_before*/,
//                const int64_t* /*padding_after*/, T) = nullptr;
//   if (launch_pack_size == 1) {
//     func = DispatchIndexType<num_dims, T, 1>;
//   } else if (launch_pack_size == 2) {
//     func = DispatchIndexType<num_dims, T, 2>;
//   } else if (launch_pack_size == 4) {
//     func = DispatchIndexType<num_dims, T, 4>;
//   } else if (launch_pack_size == 8) {
//     func = DispatchIndexType<num_dims, T, 8>;
//   } else if (launch_pack_size == 16) {
//     func = DispatchIndexType<num_dims, T, 16>;
//   } else {
//     UNIMPLEMENTED();
//   }
//   func(stream, dst, dst_dims, src, src_dims, padding_before, padding_after, pad_val);
// }

// template<typename T>
// void LaunchWithSimplified(Stream* stream, size_t num_dims, void* dst, int64_t* dst_dims,
//                           const void* src, int64_t* src_dims, int64_t* padding_before,
//                           int64_t* padding_after, T pad_val) {
//   void (*func)(Stream* /*stream*/, void* /*dst*/, int64_t* /*dst_dims*/, const void* /*src*/,
//                int64_t* /*src_dims*/, int64_t* /*padding_before*/, int64_t* /*padding_after*/, T) =
//       nullptr;
//   if (num_dims == 1) {
//     func = DispatchPackSize<1, T>;
//   } else if (num_dims == 2) {
//     func = DispatchPackSize<2, T>;
//   } else if (num_dims == 3) {
//     func = DispatchPackSize<3, T>;
//   } else if (num_dims == 4) {
//     func = DispatchPackSize<4, T>;
//   } else if (num_dims == 5) {
//     func = DispatchPackSize<5, T>;
//   } else if (num_dims == 6) {
//     func = DispatchPackSize<6, T>;
//   } else if (num_dims == 7) {
//     func = DispatchPackSize<7, T>;
//   } else if (num_dims == 8) {
//     func = DispatchPackSize<8, T>;
//   } else {
//     UNIMPLEMENTED();
//   }
//   func(stream, dst, dst_dims, src, src_dims, padding_before, padding_after, pad_val);
// }


// template<typename T>
// void SimplifyThenLaunch(Stream* stream, size_t num_dims, void* dst, const int64_t* dst_dims,
//                         const void* src, const int64_t* src_dims, const int64_t* padding_before,
//                         const int64_t* padding_after, T pad_val) {
//   CHECK_LE(num_dims, kMaxNumDims);
//   int64_t simplified_dst_dims[kMaxNumDims];
//   int64_t simplified_src_dims[kMaxNumDims];
//   int64_t simplified_padding_before[kMaxNumDims];
//   int64_t simplified_padding_after[kMaxNumDims];
//   size_t simplified_num_dims = 1;
//   SimplifyPadDims(num_dims, dst_dims, src_dims, padding_before, padding_after, &simplified_num_dims,
//                   simplified_dst_dims, simplified_src_dims, simplified_padding_before,
//                   simplified_padding_after);
//   LaunchWithSimplified<T>(stream, simplified_num_dims, dst, simplified_dst_dims, src,
//                           simplified_src_dims, simplified_padding_before, simplified_padding_after,
//                           pad_val);
// }

// template<typename T>
// class ConstantPadImpl : public ConstantPad {
//  public:
//   OF_DISALLOW_COPY_AND_MOVE(ConstantPadImpl);
//   ConstantPadImpl() = default;
//   ~ConstantPadImpl() override = default;

//   void Launch(Stream* stream, size_t num_dims, void* dst, const int64_t* dst_dims, const void* src,
//               const int64_t* src_dims, const int64_t* padding_before, const int64_t* padding_after,
//               Scalar pad_val) override {
//     SimplifyThenLaunch<T>(stream, num_dims, dst, dst_dims, src, src_dims, padding_before,
//                           padding_after, GetValue<T>(pad_val));
//   }
// };

// template<typename T>
// std::unique_ptr<ConstantPad> NewConstantPad() {
//   return std::unique_ptr<ConstantPad>(new ConstantPadImpl<T>());
// }

// #define CUDA_CONSTANT_PAD_PRIMITIVE_TYPE_SEQ \
//   CUDA_PRIMITIVE_INT32_TYPE_SEQ              \
//   CUDA_PRIMITIVE_INT64_TYPE_SEQ              \
//   CUDA_PRIMITIVE_FLOAT_TYPE_SEQ

// class ConstantPadFactoryImpl : public ConstantPadFactory {
//  public:
//   OF_DISALLOW_COPY_AND_MOVE(ConstantPadFactoryImpl);
//   ConstantPadFactoryImpl() = default;
//   ~ConstantPadFactoryImpl() override = default;

//   std::unique_ptr<ConstantPad> New(DataType data_type) override {
// #define MAKE_NEW_CONSTANT_PAD_ENTRY(type_cpp, type_proto) {type_proto, NewConstantPad<type_cpp>},

//     static const std::map<DataType, std::function<std::unique_ptr<ConstantPad>()>>
//         new_constant_pad_handle{OF_PP_FOR_EACH_TUPLE(MAKE_NEW_CONSTANT_PAD_ENTRY,
//                                                      CUDA_CONSTANT_PAD_PRIMITIVE_TYPE_SEQ)};

// #undef MAKE_NEW_CONSTANT_PAD_ENTRY

//     const auto it = new_constant_pad_handle.find(data_type);
//     if (it != new_constant_pad_handle.end()) {
//       return it->second();
//     } else {
//       return nullptr;
//     }
//   }
// };

// REGISTER_PRIMITIVE_FACTORY(DeviceType::kCUDA, ConstantPadFactory, ConstantPadFactoryImpl);

// }  // namespace

// }  // namespace primitive

// }  // namespace ep

// }  // namespace oneflow

// =====


#include "oneflow/core/ep/include/primitive/constant_pad.h"
#include "oneflow/core/ep/common/primitive/constant_pad.h"
#include "oneflow/core/ep/cuda/primitive/type_seq.h"
#include "oneflow/core/ep/cuda/cuda_stream.h"
#include <cuda_runtime.h>

namespace oneflow {

namespace ep {

namespace primitive {

namespace {

constexpr int32_t kMaxNumDims = 8;
constexpr int32_t Min(int32_t a, int32_t b) { return a < b ? a : b; }
constexpr int32_t kMaxPackBytes = 128 / 8;

template<typename T>
constexpr int32_t GetMaxPackSize() {
  return Min(kMaxPackBytes / sizeof(T), 8);
}

template<typename T, int pack_size>
struct GetPackType {
  using type = typename std::aligned_storage<pack_size * sizeof(T), pack_size * sizeof(T)>::type;
};

template<typename T, int pack_size>
using PackType = typename GetPackType<T, pack_size>::type;

template<typename T, size_t pack_size>
union Pack {
  static_assert(sizeof(PackType<T, pack_size>) == sizeof(T) * pack_size, "");
  explicit __host__ __device__ Pack(T value) {
#pragma unroll
    for (int i = 0; i < pack_size; i++) { elem[i] = value; }
  }
  T elem[pack_size];
  PackType<T, pack_size> storage;
};

template<typename IndexType, typename T, int pack_size>
__global__ void ConstantPadKernel(ConstantPadParams<kMaxNumDims, IndexType> params, T pad_value, size_t num_dims) {
  IndexType global_thread_id = blockIdx.x * blockDim.x + threadIdx.x;
  using LoadStoreType = PackType<T, pack_size>;
  const LoadStoreType* src = reinterpret_cast<const LoadStoreType*>(params.src);
  LoadStoreType* dst = reinterpret_cast<LoadStoreType*>(params.dst);
  IndexType src_index[kMaxNumDims];
  IndexType dst_index[kMaxNumDims];
  // todo: Use cuda1d kernel loop
  for (IndexType linear_index = global_thread_id; linear_index < params.elem_cnt;
       linear_index += gridDim.x * blockDim.x) {
    params.dst_index_helper.OffsetToNdIndex(linear_index, dst_index, num_dims);
    bool if_pad = false;
#pragma unroll
    for (int i = 0; i < kMaxNumDims; i++) {
      if (i >= num_dims){
        break; 
      }
      else{
        if (dst_index[i] >= params.padding_before[i]
          && dst_index[i] < params.out_size[i] - params.padding_after[i]) {
          src_index[i] = dst_index[i] - params.padding_before[i];
        } else {
          if_pad = true;
          break;
        }
      }
    }
    if (!if_pad) {
      const IndexType src_offset = params.src_index_helper.NdIndexToOffset(src_index, num_dims);
      dst[linear_index] = src[src_offset];
    } else {
      Pack<T, pack_size> packed_pad_val(pad_value);
      dst[linear_index] = packed_pad_val.storage;
    }
  }
}

template<typename T>
T GetValue(Scalar value) {
  return value.Value<T>();
}

template<>
half GetValue<half>(Scalar value) {
  return static_cast<half>(GetValue<float>(value));
}

#if CUDA_VERSION >= 11000

template<>
nv_bfloat16 GetValue<nv_bfloat16>(Scalar value) {
  return static_cast<nv_bfloat16>(GetValue<float>(value));
}

#endif  // CUDA_VERSION >= 11000

template<size_t max_pack_size>
size_t GetLaunchPackSize(size_t elem_size, size_t num_dims, void* dst, const int64_t* dst_dims,
                         const void* src, const int64_t* src_dims, const int64_t* padding_before,
                         const int64_t* padding_after) {
  static_assert(max_pack_size > 0 && (max_pack_size & (max_pack_size - 1)) == 0, "");
  CHECK_GT(elem_size, 0);
  CHECK_EQ((elem_size & (elem_size - 1)), 0);
  CHECK_EQ(max_pack_size % elem_size, 0);
  const int64_t last_dst_dim_size = dst_dims[num_dims - 1];
  const int64_t last_src_dim_size = src_dims[num_dims - 1];
  const int64_t last_padding_before_size = padding_before[num_dims - 1];
  const int64_t last_padding_after_size = padding_after[num_dims - 1];
  auto src_ptr = reinterpret_cast<std::uintptr_t>(src);
  auto dst_ptr = reinterpret_cast<std::uintptr_t>(dst);
  // todo: think padding is negative val. 
  for (size_t size = max_pack_size; size > 1; size /= 2) {
    if (last_dst_dim_size % size == 0 && last_src_dim_size % size == 0
        && last_padding_before_size % size == 0 && last_padding_after_size % size == 0
        && src_ptr % size == 0 && dst_ptr % size == 0) {
      return size;
    }
  }
  return 1;
}

constexpr int kBlockSize = 256;
constexpr int kNumWaves = 32;

inline cudaError_t GetNumBlocks(int64_t n, int* num_blocks) {
  int dev;
  {
    cudaError_t err = cudaGetDevice(&dev);
    if (err != cudaSuccess) { return err; }
  }
  int sm_count;
  {
    cudaError_t err = cudaDeviceGetAttribute(&sm_count, cudaDevAttrMultiProcessorCount, dev);
    if (err != cudaSuccess) { return err; }
  }
  int tpm;
  {
    cudaError_t err = cudaDeviceGetAttribute(&tpm, cudaDevAttrMaxThreadsPerMultiProcessor, dev);
    if (err != cudaSuccess) { return err; }
  }
  *num_blocks = std::max<int>(1, std::min<int64_t>((n + kBlockSize - 1) / kBlockSize,
                                                   sm_count * tpm / kBlockSize * kNumWaves));
  return cudaSuccess;
}

template<typename IndexType, typename T, size_t pack_size>
void LaunchKernel(Stream* stream, ConstantPadParams<kMaxNumDims, IndexType> params, T pad_val, size_t num_dims) {
  cudaStream_t cuda_stream = stream->As<CudaStream>()->cuda_stream();
  int num_blocks = 0; 
  GetNumBlocks(params.elem_cnt / pack_size, &num_blocks); 

  ConstantPadKernel<IndexType, T, pack_size>
      <<<num_blocks, kBlockSize, 0,
         cuda_stream>>>(params, pad_val, num_dims);
}

template<typename IndexType, typename T, size_t pack_size>
void LaunchKernel(Stream* stream, void* dst, const int64_t* dst_dims, const void* src,
                  const int64_t* src_dims, const int64_t* padding_before,
                  const int64_t* padding_after, T pad_val, size_t num_dims) {
  ConstantPadParams<kMaxNumDims, IndexType> params;
  params.dst_index_helper = NdIndexOffsetHelper<IndexType, kMaxNumDims>(dst_dims, num_dims);
  params.src_index_helper = NdIndexOffsetHelper<IndexType, kMaxNumDims>(src_dims, num_dims);
  params.dst = dst;
  params.src = src;
  size_t elem_cnt = 1;
  for (int i = 0; i < num_dims; i++) {
    params.padding_before[i] = padding_before[i];
    params.padding_after[i] = padding_after[i];
    params.out_size[i] = dst_dims[i];
    elem_cnt *= params.out_size[i];
  }
  params.elem_cnt = elem_cnt;
  LaunchKernel<IndexType, T, pack_size>(stream, params, pad_val, num_dims);
}

template<typename T, size_t pack_size>
void DispatchIndexType(Stream* stream, void* dst, const int64_t* dst_dims, const void* src,
                       const int64_t* src_dims, const int64_t* padding_before,
                       const int64_t* padding_after, T pad_val, size_t num_dims) {
  size_t elem_cnt = 1;
  for (size_t i = 0; i < num_dims; ++i) { elem_cnt *= dst_dims[i]; }
  if (elem_cnt < GetMaxVal<int32_t>()) {
    LaunchKernel<int32_t, T, pack_size>(stream, dst, dst_dims, src, src_dims,
                                                  padding_before, padding_after, pad_val, num_dims);
  } else {
    LaunchKernel<int64_t, T, pack_size>(stream, dst, dst_dims, src, src_dims,
                                                  padding_before, padding_after, pad_val, num_dims);
  }
}

void SimplifyPadDims(size_t num_dims, const int64_t* dst_dims, const int64_t* src_dims,
                     const int64_t* padding_before, const int64_t* padding_after,
                     size_t* simplified_num_dims, int64_t* simplified_dst_dims,
                     int64_t* simplified_src_dims, int64_t* simplified_padding_before,
                     int64_t* simplified_padding_after) {
  CHECK_NE(num_dims, 0);
  size_t valid_num_dims = 0;
  FOR_RANGE(size_t, i, 0, num_dims) {
    if ((i != 0) && (dst_dims[i] == src_dims[i])) {
      simplified_dst_dims[valid_num_dims - 1] *= dst_dims[i];
      simplified_src_dims[valid_num_dims - 1] *= src_dims[i];
      simplified_padding_before[valid_num_dims - 1] *= src_dims[i];
      simplified_padding_after[valid_num_dims - 1] *= src_dims[i];
    } else {
      simplified_dst_dims[valid_num_dims] = dst_dims[i];
      simplified_src_dims[valid_num_dims] = src_dims[i];
      simplified_padding_before[valid_num_dims] = padding_before[i];
      simplified_padding_after[valid_num_dims] = padding_after[i];
      valid_num_dims += 1;
    }
  }
  *simplified_num_dims = valid_num_dims;
}

template<typename T>
void DispatchPackSize(Stream* stream, void* dst, int64_t* dst_dims, const void* src,
                      int64_t* src_dims, int64_t* padding_before, int64_t* padding_after,
                      T pad_val, size_t num_dims) {
  constexpr int32_t max_packsize = GetMaxPackSize<T>();
  size_t launch_pack_size = GetLaunchPackSize<max_packsize>(
      sizeof(T), num_dims, dst, dst_dims, src, src_dims, padding_before, padding_after);

  printf("simplified num dims is: %ld \n", num_dims);
  printf("Launch pack size is: %ld \n", launch_pack_size);
  for (int i = 0; i < num_dims; i++) {
    printf("simplified_dst_dims %d is: %ld \n", i, dst_dims[i]);
  }
  for (int i = 0; i < num_dims; i++) {
    printf("simplified_src_dims %d is: %ld \n", i, src_dims[i]);
  }
  for (int i = 0; i < num_dims; i++) {
    printf("padding before %d is: %ld \n", i, padding_before[i]);
  }
  for (int i = 0; i < num_dims; i++) { printf("padding after %d is: %ld \n", i, padding_after[i]); }

  dst_dims[num_dims - 1] /= launch_pack_size;
  src_dims[num_dims - 1] /= launch_pack_size;
  padding_before[num_dims - 1] /= launch_pack_size;
  padding_after[num_dims - 1] /= launch_pack_size;

  void (*func)(Stream* /*stream*/, void* /*dst*/, const int64_t* /*dst_dims*/, const void* /*src*/,
               const int64_t* /*src_dims*/, const int64_t* /*padding_before*/,
               const int64_t* /*padding_after*/, T, size_t) = nullptr;
  if (launch_pack_size == 1) {
    func = DispatchIndexType<T, 1>;
  } else if (launch_pack_size == 2) {
    func = DispatchIndexType<T, 2>;
  } else if (launch_pack_size == 4) {
    func = DispatchIndexType<T, 4>;
  } else if (launch_pack_size == 8) {
    func = DispatchIndexType<T, 8>;
  } else if (launch_pack_size == 16) {
    func = DispatchIndexType<T, 16>;
  } else {
    UNIMPLEMENTED();
  }
  func(stream, dst, dst_dims, src, src_dims, padding_before, padding_after, pad_val, num_dims);
}

template<typename T>
void LaunchWithSimplified(Stream* stream, size_t num_dims, void* dst, int64_t* dst_dims,
                          const void* src, int64_t* src_dims, int64_t* padding_before,
                          int64_t* padding_after, T pad_val) {
  void (*func)(Stream* /*stream*/, void* /*dst*/, int64_t* /*dst_dims*/, const void* /*src*/,
               int64_t* /*src_dims*/, int64_t* /*padding_before*/, int64_t* /*padding_after*/, T, size_t) =
      nullptr;
  func = DispatchPackSize<T>;
  func(stream, dst, dst_dims, src, src_dims, padding_before, padding_after, pad_val, num_dims);
}


template<typename T>
void SimplifyThenLaunch(Stream* stream, size_t num_dims, void* dst, const int64_t* dst_dims,
                        const void* src, const int64_t* src_dims, const int64_t* padding_before,
                        const int64_t* padding_after, T pad_val) {
  CHECK_LE(num_dims, kMaxNumDims);
  int64_t simplified_dst_dims[kMaxNumDims];
  int64_t simplified_src_dims[kMaxNumDims];
  int64_t simplified_padding_before[kMaxNumDims];
  int64_t simplified_padding_after[kMaxNumDims];
  size_t simplified_num_dims = 1;
  SimplifyPadDims(num_dims, dst_dims, src_dims, padding_before, padding_after, &simplified_num_dims,
                  simplified_dst_dims, simplified_src_dims, simplified_padding_before,
                  simplified_padding_after);
  LaunchWithSimplified<T>(stream, simplified_num_dims, dst, simplified_dst_dims, src,
                          simplified_src_dims, simplified_padding_before, simplified_padding_after,
                          pad_val);
}

template<typename T>
class ConstantPadImpl : public ConstantPad {
 public:
  OF_DISALLOW_COPY_AND_MOVE(ConstantPadImpl);
  ConstantPadImpl() = default;
  ~ConstantPadImpl() override = default;

  void Launch(Stream* stream, size_t num_dims, void* dst, const int64_t* dst_dims, const void* src,
              const int64_t* src_dims, const int64_t* padding_before, const int64_t* padding_after,
              Scalar pad_val) override {
    SimplifyThenLaunch<T>(stream, num_dims, dst, dst_dims, src, src_dims, padding_before,
                          padding_after, GetValue<T>(pad_val));
  }
};

template<typename T>
std::unique_ptr<ConstantPad> NewConstantPad() {
  return std::unique_ptr<ConstantPad>(new ConstantPadImpl<T>());
}

#define CUDA_CONSTANT_PAD_PRIMITIVE_TYPE_SEQ \
  CUDA_PRIMITIVE_INT32_TYPE_SEQ              \
  CUDA_PRIMITIVE_INT64_TYPE_SEQ              \
  CUDA_PRIMITIVE_FLOAT_TYPE_SEQ

class ConstantPadFactoryImpl : public ConstantPadFactory {
 public:
  OF_DISALLOW_COPY_AND_MOVE(ConstantPadFactoryImpl);
  ConstantPadFactoryImpl() = default;
  ~ConstantPadFactoryImpl() override = default;

  std::unique_ptr<ConstantPad> New(DataType data_type) override {
#define MAKE_NEW_CONSTANT_PAD_ENTRY(type_cpp, type_proto) {type_proto, NewConstantPad<type_cpp>},

    static const std::map<DataType, std::function<std::unique_ptr<ConstantPad>()>>
        new_constant_pad_handle{OF_PP_FOR_EACH_TUPLE(MAKE_NEW_CONSTANT_PAD_ENTRY,
                                                     CUDA_CONSTANT_PAD_PRIMITIVE_TYPE_SEQ)};

#undef MAKE_NEW_CONSTANT_PAD_ENTRY

    const auto it = new_constant_pad_handle.find(data_type);
    if (it != new_constant_pad_handle.end()) {
      return it->second();
    } else {
      return nullptr;
    }
  }
};

REGISTER_PRIMITIVE_FACTORY(DeviceType::kCUDA, ConstantPadFactory, ConstantPadFactoryImpl);

}  // namespace

}  // namespace primitive

}  // namespace ep

}  // namespace oneflow
