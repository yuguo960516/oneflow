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
#include "oneflow/core/framework/framework.h"

namespace oneflow {

namespace {

Maybe<void> InferTensorDesc4MatmulBiasadd(user_op::InferContext* ctx) {
  bool transpose_a = ctx->Attr<bool>("transpose_a");
  bool transpose_b = ctx->Attr<bool>("transpose_b");

  user_op::TensorDesc* a = ctx->TensorDesc4ArgNameAndIndex("a", 0);
  user_op::TensorDesc* b = ctx->TensorDesc4ArgNameAndIndex("b", 0);
  user_op::TensorDesc* bias = ctx->TensorDesc4ArgNameAndIndex("bias", 0);
  const auto bias_add_axis = ctx->Attr<int32_t>("axis");
  CHECK_EQ_OR_RETURN(a->shape().NumAxes(), b->shape().NumAxes());
  CHECK_GE_OR_RETURN(a->shape().NumAxes(), 2);
  size_t num_axes = a->shape().NumAxes();

  if (num_axes > 2) {
    for (int i = 0; i < num_axes - 2; ++i) {
      CHECK_EQ_OR_RETURN(a->shape().At(i), b->shape().At(i));
    }
  }

  user_op::TensorDesc* out = ctx->TensorDesc4ArgNameAndIndex("out", 0);
  *out = *a;
  int64_t m, n, k;  // tensor a (no trans): m*k, tensor b (no trans): k*n
  if (!transpose_a) {
    m = a->shape().At(num_axes - 2);
    k = a->shape().At(num_axes - 1);
  } else {
    m = a->shape().At(num_axes - 1);
    k = a->shape().At(num_axes - 2);
  }
  if (!transpose_b) {
    CHECK_EQ_OR_RETURN(k, b->shape().At(num_axes - 2));
    n = b->shape().At(num_axes - 1);
  } else {
    CHECK_EQ_OR_RETURN(k, b->shape().At(num_axes - 1));
    n = b->shape().At(num_axes - 2);
  }
  out->mut_shape()->Set(num_axes - 2, m);
  out->mut_shape()->Set(num_axes - 1, n);

  CHECK_EQ_OR_RETURN(bias->shape().NumAxes(), 1);
  CHECK_GE_OR_RETURN(bias_add_axis, 0);
  CHECK_LT_OR_RETURN(bias_add_axis, out->shape().NumAxes());
  CHECK_EQ_OR_RETURN(out->shape().At(bias_add_axis), bias->shape().At(0));

  if (ctx->user_op_conf().has_input("_add_to_output", 0)) {
    const auto* add_to_output = ctx->TensorDesc4ArgNameAndIndex("_add_to_output", 0);
    CHECK_EQ_OR_RETURN(add_to_output->data_type(), out->data_type());
    CHECK_EQ_OR_RETURN(add_to_output->shape(), out->shape());
  }
  return Maybe<void>::Ok();
}

void GenBackwardOpConf4MatmulBiasadd(const std::string& op_type_name, const user_op::UserOpWrapper& op,
                              user_op::AddOpFn AddOp) {
  bool transpose_a = op.attr<bool>("transpose_a");
  bool transpose_b = op.attr<bool>("transpose_b");
  auto HandleGradOp = [&](user_op::UserOpConfWrapper&& grad_op,
                          std::string&& input_arg_name) -> void {
    op.BindGradTensorWithOpInput(grad_op.output("out", 0), input_arg_name, 0);
    AddOp(grad_op);
  };

  if (op.NeedGenGradTensor4OpInput("a", 0)) {
    if (transpose_a) {
      user_op::UserOpConfWrapper grad_a_op =
          user_op::UserOpConfWrapperBuilder(op.op_name() + "_grad_a")
              .Op(op_type_name)
              .Input("a", op.input("b", 0))
              .Input("b", op.GetGradTensorWithOpOutput("out", 0))
              .Output("out")
              .Attr<bool>("transpose_a", transpose_b)
              .Attr<bool>("transpose_b", true)
              .Build();
      HandleGradOp(std::move(grad_a_op), "a");
    } else {
      user_op::UserOpConfWrapper grad_a_op =
          user_op::UserOpConfWrapperBuilder(op.op_name() + "_grad_a")
              .Op(op_type_name)
              .Input("a", op.GetGradTensorWithOpOutput("out", 0))
              .Input("b", op.input("b", 0))
              .Output("out")
              .Attr<bool>("transpose_a", false)
              .Attr<bool>("transpose_b", !transpose_b)
              .Build();
      HandleGradOp(std::move(grad_a_op), "a");
    }
  }
  if (op.NeedGenGradTensor4OpInput("b", 0)) {
    if (transpose_b) {
      user_op::UserOpConfWrapper grad_b_op =
          user_op::UserOpConfWrapperBuilder(op.op_name() + "_grad_b")
              .Op(op_type_name)
              .Input("a", op.GetGradTensorWithOpOutput("out", 0))
              .Input("b", op.input("a", 0))
              .Output("out")
              .Attr<bool>("transpose_a", true)
              .Attr<bool>("transpose_b", transpose_a)
              .Build();
      HandleGradOp(std::move(grad_b_op), "b");
    } else {
      user_op::UserOpConfWrapper grad_b_op =
          user_op::UserOpConfWrapperBuilder(op.op_name() + "_grad_b")
              .Op(op_type_name)
              .Input("a", op.input("a", 0))
              .Input("b", op.GetGradTensorWithOpOutput("out", 0))
              .Output("out")
              .Attr<bool>("transpose_a", !transpose_a)
              .Attr<bool>("transpose_b", false)
              .Build();
      HandleGradOp(std::move(grad_b_op), "b");
    }
  }
  if (op.NeedGenGradTensor4OpInput("bias", 0)) {
    const int64_t num_axes = op.TensorDesc4ArgNameAndIndex("a", 0).shape().NumAxes();
    const int32_t bias_add_axis = op.attr<int32_t>("axis");
    std::vector<int32_t> reduce_axes_vec;
    FOR_RANGE(int64_t, i, 0, num_axes) {
      if (i != bias_add_axis) { reduce_axes_vec.push_back(i); }
    }
    user_op::UserOpConfWrapperBuilder builder(op.op_name() + "_grad");
    auto grad_op = builder.Op("reduce_sum")
                        .Input("input_tensor", op.GetGradTensorWithOpOutput("out", 0))
                        .Output("output_tensor")
                        .Attr("axis", reduce_axes_vec)
                        .Attr("keepdims", false)
                        .Build();
    AddOp(grad_op);
    op.BindGradTensorWithOpInput(grad_op.output("output_tensor", 0), "bias", 0);
  }
}

}  // namespace

REGISTER_USER_OP("matmul_biasadd")
    .Input("a")
    .Input("b")
    .OptionalInput("_add_to_output")
    .Input("bias")
    .Output("out")
    .Attr<bool>("transpose_a", false)
    .Attr<bool>("transpose_b", false)
    .Attr<int32_t>("axis")
    .SetTensorDescInferFn(InferTensorDesc4MatmulBiasadd)
    .SetBatchAxisInferFn([](user_op::BatchAxisContext* ctx) -> Maybe<void> {
      auto BatchAxis4BnInOp = [&ctx](const std::string& arg_name) -> OptInt64* {
        return ctx->BatchAxis4ArgNameAndIndex(arg_name, 0);
      };
      OptInt64 a_batch_axis(*BatchAxis4BnInOp("a"));
      if (a_batch_axis.has_value() && ctx->Attr<bool>("transpose_a")) {
        a_batch_axis.set_value(1 - a_batch_axis.value());
      }
      OptInt64 b_batch_axis(*BatchAxis4BnInOp("b"));
      if (b_batch_axis.has_value() && ctx->Attr<bool>("transpose_b")) {
        b_batch_axis.set_value(1 - b_batch_axis.value());
      }
      if (a_batch_axis.has_value() && a_batch_axis.value() == 0) {
        *BatchAxis4BnInOp("out") = a_batch_axis;
      } else if (b_batch_axis.has_value() && b_batch_axis.value() == 1) {
        *BatchAxis4BnInOp("out") = b_batch_axis;
      } else {
        BatchAxis4BnInOp("out")->clear_value();
      }
      return Maybe<void>::Ok();
    })
    .SetGetSbpFn([](user_op::SbpContext* ctx) -> Maybe<void> {
      const user_op::TensorDesc& a_tensor = ctx->LogicalTensorDesc4InputArgNameAndIndex("a", 0);
      std::vector<user_op::OpArg> out_and_add_to_output_args;
      out_and_add_to_output_args.emplace_back("out", 0);
      if (ctx->user_op_conf().has_input("_add_to_output", 0)) {
          out_and_add_to_output_args.emplace_back("_add_to_output", 0);
      }
      size_t num_axis = a_tensor.shape().NumAxes();
      // (m, k_a) * (k_b, n) where k_a == k_b
      int32_t m_axis = -1;
      int32_t k_a_axis = -1;
      int32_t k_b_axis = -1;
      int32_t n_axis = -1;
      if (ctx->Attr<bool>("transpose_a")) {
        m_axis = 1;
        k_a_axis = 0;
      } else {
        m_axis = 0;
        k_a_axis = 1;
      }
      if (ctx->Attr<bool>("transpose_b")) {
        k_b_axis = 1;
        n_axis = 0;
      } else {
        k_b_axis = 0;
        n_axis = 1;
      }
      const auto axis = ctx->Attr<int32_t>("axis");

      if (0 == axis) {
          ctx->NewBuilder()
          .Split(user_op::OpArg("a", 0), m_axis)
          .Broadcast(user_op::OpArg("b", 0))
          .Split(out_and_add_to_output_args, 0)
          .Split(user_op::OpArg("bias", 0), axis)
          .Build();
          ctx->NewBuilder()
          .Broadcast(user_op::OpArg("a", 0))
          .Split(user_op::OpArg("b", 0), n_axis)
          .Split(out_and_add_to_output_args, 1)
          .Broadcast(user_op::OpArg("bias", 0))
          .Build();
      } else {
          ctx->NewBuilder()
          .Split(user_op::OpArg("a", 0), m_axis)
          .Broadcast(user_op::OpArg("b", 0))
          .Split(out_and_add_to_output_args, 0)
          .Broadcast(user_op::OpArg("bias", 0))
          .Build();
          ctx->NewBuilder()
          .Broadcast(user_op::OpArg("a", 0))
          .Split(user_op::OpArg("b", 0), n_axis)
          .Split(out_and_add_to_output_args, 1)
          .Split(user_op::OpArg("bias", 0), axis)
          .Build();
      }
      ctx->NewBuilder()
          .Split(user_op::OpArg("a", 0), k_a_axis)
          .Split(user_op::OpArg("b", 0), k_b_axis)
          .PartialSum(out_and_add_to_output_args)
          .PartialSum(user_op::OpArg("bias", 0))
          .Build();
      ctx->NewBuilder()
          .PartialSum(user_op::OpArg("a", 0))
          .Broadcast(user_op::OpArg("b", 0))
          .PartialSum(out_and_add_to_output_args)
          .PartialSum(user_op::OpArg("bias", 0))
          .Build();
      ctx->NewBuilder()
          .Broadcast(user_op::OpArg("a", 0))
          .PartialSum(user_op::OpArg("b", 0))
          .PartialSum(out_and_add_to_output_args)
          .PartialSum(user_op::OpArg("bias", 0))
          .Build();
    });

REGISTER_USER_OP_GRAD("matmul_biasadd").SetGenBackwardOpConfFn([](const user_op::UserOpWrapper& op,
                                                          user_op::AddOpFn AddOp) {
    return GenBackwardOpConf4MatmulBiasadd("matmul_biasadd", op, AddOp);
});


REGISTER_USER_OP("batch_matmul_biasadd")
    .Input("a")
    .Input("b")
    .OptionalInput("_add_to_output")
    .Input("bias")
    .Output("out")
    .Attr<bool>("transpose_a", false)
    .Attr<bool>("transpose_b", false)
    .Attr<int32_t>("axis")
    .SetTensorDescInferFn(InferTensorDesc4MatmulBiasadd)
    .SetBatchAxisInferFn([](user_op::BatchAxisContext* ctx) -> Maybe<void> {
      auto BatchAxis4BnInOp = [&ctx](const std::string& arg_name) -> OptInt64* {
        return ctx->BatchAxis4ArgNameAndIndex(arg_name, 0);
      };
      if (BatchAxis4BnInOp("a")->has_value()) {
        *BatchAxis4BnInOp("out") = *BatchAxis4BnInOp("a");
      } else if (BatchAxis4BnInOp("b")->has_value()) {
        *BatchAxis4BnInOp("out") = *BatchAxis4BnInOp("b");
      } else {
        BatchAxis4BnInOp("out")->clear_value();
      }
      return Maybe<void>::Ok();
    })
    .SetGetSbpFn([](user_op::SbpContext* ctx) -> Maybe<void> {
      const user_op::TensorDesc& a_tensor = ctx->LogicalTensorDesc4InputArgNameAndIndex("a", 0);
      std::vector<user_op::OpArg> out_and_add_to_output_args;
      const auto axis = ctx->Attr<int32_t>("axis");
      out_and_add_to_output_args.emplace_back("out", 0);
      if (ctx->user_op_conf().has_input("_add_to_output", 0)) {
        out_and_add_to_output_args.emplace_back("_add_to_output", 0);
      }
      FOR_RANGE(int64_t, i, 0, a_tensor.shape().NumAxes() - 2) {
        if (i == axis) {
          ctx->NewBuilder()
          .Split(user_op::OpArg("a", 0), i)
          .Split(user_op::OpArg("b", 0), i)
          .Split(out_and_add_to_output_args, i)
          .Split(user_op::OpArg("bias", 0), 0)
          .Build();
          continue; 
        }
        ctx->NewBuilder()
        .Split(user_op::OpArg("a", 0), i)
        .Split(user_op::OpArg("b", 0), i)
        .Split(out_and_add_to_output_args, i)
        .Broadcast(user_op::OpArg("bias", 0))
        .Build();
      }
      return Maybe<void>::Ok();
    });

REGISTER_USER_OP_GRAD("batch_matmul")
    .SetGenBackwardOpConfFn([](const user_op::UserOpWrapper& op, user_op::AddOpFn AddOp) {
      return GenBackwardOpConf4MatmulBiasadd("batch_matmul", op, AddOp);
    });

}  // namespace oneflow