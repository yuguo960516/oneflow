#include "oneflow/core/operator/pooling_op.h"

namespace oneflow {

void PoolingOp::InitFromOpConf() {
  std::string padding_mthd = GetValFromCustomizedConf<std::string>("padding");
  std::transform(padding_mthd.begin(), padding_mthd.end(), padding_mthd.begin(),
                 ::tolower);
  if (padding_mthd != "same" && padding_mthd != "valid") {
    LOG(FATAL) << "Invalid padding method in " << op_name();
  }
  SetValInCustomizedConf("padding", padding_mthd);
  std::string data_format =
      GetValFromCustomizedConf<std::string>("data_format");
  std::transform(data_format.begin(), data_format.end(), data_format.begin(),
                 ::tolower);
  if (data_format != "channels_last" && data_format != "channels_first") {
    LOG(FATAL) << "Invalid data format in " << op_name();
  }
  SetValInCustomizedConf("data_format", data_format);
  CheckPoolSizeAndStrides();
  EnrollInputBn("in");
  EnrollOutputBn("out");
}

void PoolingOp::InferBlobDescs(
    std::function<BlobDesc*(const std::string)> GetBlobDesc4BnInOp,
    const ParallelContext* parallel_ctx) const {
  // in
  const BlobDesc* in_blob_desc = GetBlobDesc4BnInOp("in");
  const Shape& in_shape = in_blob_desc->shape();
  CHECK_GE(in_blob_desc->shape().NumAxes(), 3);
  CHECK_LE(in_blob_desc->shape().NumAxes(), 5);
  CHECK_EQ(in_blob_desc->data_type(), JobDesc::Singleton()->DefaultDataType());
  // out
  std::vector<int64_t> in = {GetInDim(in_shape, 0), GetInDim(in_shape, 1),
                             GetInDim(in_shape, 2)};
  std::vector<int32_t> pool_size = Get3DVecInOpConf("pool_size");
  std::vector<int32_t> strides = Get3DVecInOpConf("strides");
  std::vector<int64_t> out;
  Get3DOutputSize(in, pool_size, strides,
                  GetValFromCustomizedConf<std::string>("padding"), &out,
                  nullptr);

  BlobDesc* out_blob_desc = GetBlobDesc4BnInOp("out");
  *out_blob_desc = *in_blob_desc;
  out_blob_desc->mut_shape() = GetOutShape(in_shape.At(0), in_shape.At(1), out);
}

void PoolingOp::CheckPoolSizeAndStrides() const {
  const PbRf<int32_t>& pool_size =
      GetPbRfFromCustomizedConf<int32_t>("pool_size");
  CHECK_EQ(pool_size.size(), GetDim());
  for (int32_t pool_dim : pool_size) { CHECK_GT(pool_dim, 0); }
  const PbRf<int32_t>& strides = GetPbRfFromCustomizedConf<int32_t>("strides");
  CHECK_EQ(strides.size(), GetDim());
  for (int32_t stride_dim : strides) { CHECK_GT(stride_dim, 0); }
}

std::vector<int32_t> PoolingOp::Get3DVecInOpConf(
    const std::string& field_name) const {
  std::vector<int32_t> vec;
  FOR_RANGE(uint8_t, dim, 0, 3) {
    int64_t index = static_cast<int32_t>(dim) - (3 - GetDim());
    if (index < 0) {
      vec.push_back(1);
    } else {
      vec.push_back(GetPbRfFromCustomizedConf<int32_t>(field_name).Get(index));
    }
  }
  return vec;
}

int64_t PoolingOp::GetInDim(const Shape& in_shape, uint8_t dim) const {
  int64_t offset = 0;
  std::string data_format =
      GetValFromCustomizedConf<std::string>("data_format");
  if (data_format == "channels_last") {
    offset = 1;
  } else if (data_format == "channels_first") {
    offset = 2;
  } else {
    UNIMPLEMENTED();
  }
  int64_t index = offset + static_cast<int64_t>(dim) - (3 - GetDim());
  if (index < offset) {
    return 1;
  } else {
    return in_shape.At(index);
  }
}

Shape PoolingOp::GetOutShape(int64_t in_n, int64_t in_c,
                             const std::vector<int64_t>& out) const {
  std::vector<int64_t> out_shape;
  if (GetDim() == 1) {
    out_shape = {out.at(2)};
  } else if (GetDim() == 2) {
    out_shape = {out.at(1), out.at(2)};
  } else if (GetDim() == 3) {
    out_shape = {out.at(0), out.at(1), out.at(2)};
  } else {
    UNIMPLEMENTED();
  }
  std::string data_format =
      GetValFromCustomizedConf<std::string>("data_format");
  if (data_format == "channels_first") {
    out_shape.insert(out_shape.begin(), in_c);
  } else if (data_format == "channels_last") {
    out_shape.insert(out_shape.end(), in_c);
  } else {
    UNIMPLEMENTED();
  }
  out_shape.insert(out_shape.begin(), in_n);
  return Shape(out_shape);
}

void PoolingOp::VirtualGenKernelConf(
    std::function<const BlobDesc*(const std::string&)> GetBlobDesc4BnInOp,
    const ParallelContext* parallel_ctx, KernelConf* kernel_conf) const {
  const Shape& in_shape = GetBlobDesc4BnInOp("in")->shape();
  std::vector<int64_t> in = {GetInDim(in_shape, 0), GetInDim(in_shape, 1),
                             GetInDim(in_shape, 2)};
  std::vector<int32_t> pool_size = Get3DVecInOpConf("pool_size");
  std::vector<int32_t> strides = Get3DVecInOpConf("strides");
  std::vector<int64_t> out;
  std::vector<int32_t> padding_before;
  std::vector<int32_t> padding_after;
  Get3DOutputSize(in, pool_size, strides,
                  GetValFromCustomizedConf<std::string>("padding"), &out,
                  &padding_before, &padding_after);

  auto pooling_conf = MutableMsgInCustomizedKernelConf<PoolingKernelConf>(
      kernel_conf, "pooling_conf");
  FOR_RANGE(size_t, i, 0, 3) {
    pooling_conf->mutable_pool_size()->Add(pool_size.at(i));
    pooling_conf->mutable_strides()->Add(strides.at(i));
    pooling_conf->mutable_padding_before()->Add(padding_before.at(i));
    pooling_conf->mutable_padding_after()->Add(padding_after.at(i));
  }
  std::string data_format =
      GetValFromCustomizedConf<std::string>("data_format");
  if (data_format == "channels_first") {
    Shape({in_shape.At(0), in_shape.At(1), in.at(0), in.at(1), in.at(2)})
        .ToProto(pooling_conf->mutable_in());
    Shape({in_shape.At(0), in_shape.At(1), out.at(0), out.at(1), out.at(2)})
        .ToProto(pooling_conf->mutable_out());
  } else if (data_format == "channels_last") {
    Shape({in_shape.At(0), in.at(0), in.at(1), in.at(2),
           in_shape.At(in_shape.NumAxes() - 1)})
        .ToProto(pooling_conf->mutable_in());
    Shape({in_shape.At(0), out.at(0), out.at(1), out.at(2),
           in_shape.At(in_shape.NumAxes() - 1)})
        .ToProto(pooling_conf->mutable_out());
  } else {
    UNIMPLEMENTED();
  }
  pooling_conf->set_data_format(
      GetValFromCustomizedConf<std::string>("data_format"));
}

}  // namespace oneflow
