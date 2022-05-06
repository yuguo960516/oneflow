#include "oneflow/core/ep/include/primitive/pad.h"
#include "oneflow/core/ep/common/primitive/pad.h"
#include "oneflow/core/ep/cpu/primitive/type_seq.h"


namespace oneflow { 

namespace ep {

namespace primitive {

namespace {

template<typename T, int pack_size>
struct GetPackType {
  using type = typename std::aligned_storage<pack_size * sizeof(T), pack_size * sizeof(T)>::type;
};

template<typename T, int pack_size>
using PackType = typename GetPackType<T, pack_size>::type;

template<typename T, size_t pack_size>
union Pack{
    static_assert(sizeof(PackType<T, pack_size>) == sizeof(T) * pack_size, "");
    explicit Pack(T value){
        #pragma unroll 
        for(int i = 0; i < pack_size; i++){
            elem[i] = value; 
        }
    }
    T elem[pack_size]; 
    PackType<T, pack_size> storage; 
}; 

template<size_t num_dims, typename IndexType, typename T, int pack_size>
void PadKernel(PadParams<num_dims, IndexType> params, T pad_value){
    using LoadStoreType = PackType<T, pack_size>; 
    const LoadStoreType* src = reinterpret_cast<const LoadStoreType*>(params.src);
    LoadStoreType* dst = reinterpret_cast<LoadStoreType*>(params.dst);
    IndexType src_index[num_dims];
    IndexType dst_index[num_dims];
    for (IndexType linear_index = 0; linear_index < params.elem_cnt; ++linear_index){
        params.dst_index_helper.OffsetToNdIndex(linear_index, dst_index);
        bool if_pad = false; 
        #pragma unroll 
        for(int i = 0; i < num_dims; i++){
            if (dst_index[i] >= params.padding_before[i] && 
                dst_index[i] <= params.out_size[i] - params.padding_after[i]){
                    src_index[i] = dst_index[i] - params.padding_before[i]; 
                }
            else{ 
                if_pad = true; 
                break; 
            }
        }
        if(!if_pad){
            const IndexType src_offset = params.src_index_helper.NdIndexToOffset(src_index);
            dst[linear_index] = src[src_offset]; 
        } else {
            LoadStoreType packed_pad_val(pad_value);
            dst[linear_index] = packed_pad_val; 
        }
    }
}

template<typename T>
T GetValue(Scalar value) {
  return value.Value<T>();
}

template<>
float16 GetValue<float16>(Scalar value) {
  return static_cast<float16>(GetValue<float>(value));
}

template<size_t num_dims, typename IndexType, typename T>
void LaunchKernel(Stream* stream, PadParams<num_dims, IndexType> params, T pad_val) {
  PadKernel<num_dims, IndexType, T, /*pack_size*/1>(params, pad_val);
}

template<typename T>
class PadImpl : public Pad {
 public:
  OF_DISALLOW_COPY_AND_MOVE(PadImpl);
  PadImpl() = default;
  ~PadImpl() override = default;

  void Launch(Stream* stream, size_t num_dims, void* dst,
              const int64_t* dst_dims, const void* src,
              const int64_t* src_dims, const int64_t* padding_before,
              const int64_t* padding_after, Scalar pad_val) override {
        LaunchWithSimplified<T>(stream, num_dims, dst,
                                dst_dims, src,
                                src_dims, padding_before,
                                padding_after, GetValue<T>(pad_val)); 
    }
};

template<typename T>
std::unique_ptr<Pad> NewPad() {
  return std::unique_ptr<Pad>(new PadImpl<T>());
}


#define CPU_PAD_PRIMITIVE_TYPE_SEQ \
  CPU_PRIMITIVE_INT32_TYPE_SEQ     \
  CPU_PRIMITIVE_INT64_TYPE_SEQ     \
  CPU_PRIMITIVE_FLOAT_TYPE_SEQ     \
  CPU_PRIMITIVE_DOUBLE_TYPE_SEQ 

class PadFactoryImpl : public PadFactory {
public:
 OF_DISALLOW_COPY_AND_MOVE(PadFactoryImpl);
 PadFactoryImpl() = default;
 ~PadFactoryImpl() override = default;

 std::unique_ptr<Pad> New(DataType data_type) override {
#define MAKE_NEW_Pad_ENTRY(type_cpp, type_proto) {type_proto, NewPad<type_cpp>},

   static const std::map<DataType, std::function<std::unique_ptr<Pad>()>> new_Pad_handle{
       OF_PP_FOR_EACH_TUPLE(MAKE_NEW_Pad_ENTRY, CPU_PAD_PRIMITIVE_TYPE_SEQ)};

#undef MAKE_NEW_Pad_ENTRY

   const auto it = new_Pad_handle.find(data_type);
   if (it != new_Pad_handle.end()) {
     return it->second();
   } else {
     return nullptr;
   }
 }
};

REGISTER_PRIMITIVE_FACTORY(DeviceType::kCPU, PadFactory, PadFactoryImpl);

} // namespace 

} // primitive 

} // namespace ep 

} // namespace oneflow 
