// Copyright 2018 The MACE Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "gtest/gtest.h"
#include "micro/ops/gtest_utils.h"
#include "micro/ops/matmul.h"
#include "micro/ops/cmsis_nn/arm_mat_mul_int8.h"
#include "micro/ops/substitute_op.h"
#include "micro/ops/test_utils.h"
#include "micro/ops/test_quantize_utils.h"

namespace micro {
namespace ops {
namespace test {

class MatMulOpTest : public ::testing::Test {};

namespace {

void Simple(
    const float *input0, const int32_t *input0_dims,
    const int32_t input0_dim_size,
    const float *input1, const int32_t *input1_dims,
    const int32_t input1_dim_size,
    float *output, int32_t *output_dims, const int32_t output_dim_size,
    const float *expect, const int32_t *expect_dims) {
  MatMulOp mat_mul_op;
  framework::SubstituteOp substitude_op;
  substitude_op.AddInput(input0, input0_dims, input0_dim_size)
      .AddInput(input1, input1_dims, input1_dim_size)
      .AddOutput(output, output_dims, output_dim_size);

  mat_mul_op.Init(NULL, reinterpret_cast<framework::OpContext *>(
      &substitude_op), NULL);
  mat_mul_op.Run();

  ExpectTensorNear<float>(output, output_dims, output_dim_size,
                          expect, expect_dims, output_dim_size, 1e-5);
}

void Simple1() {
  const float input0[6] = {1, 2, 3, 4, 5, 6};
  const int32_t input0_dim_size = 3;
  const int32_t input0_dims[input0_dim_size] = {1, 2, 3};
  const float input1[6] = {1, 2, 3, 4, 5, 6};
  const int32_t input1_dim_size = 3;
  const int32_t input1_dims[input1_dim_size] = {1, 3, 2};
  float output[6] = {0};
  const int32_t output_dim_size = 3;
  int32_t output_dims[output_dim_size] = {0};
  const float expect[4] = {22, 28, 49, 64};
  const int32_t expect_dims[output_dim_size] = {1, 2, 2};
  Simple(input0, input0_dims, input0_dim_size,
         input1, input1_dims, input1_dim_size,
         output, output_dims, output_dim_size,
         expect, expect_dims);
}

void Simple2() {
  const float input0[25] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
                            14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25};
  const int32_t input0_dim_size = 3;
  const int32_t input0_dims[input0_dim_size] = {1, 5, 5};
  const float input1[25] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
                            14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25};
  const int32_t input1_dim_size = 3;
  const int32_t input1_dims[input1_dim_size] = {1, 5, 5};
  float output[25] = {0};
  const int32_t output_dim_size = 3;
  int32_t output_dims[output_dim_size] = {0};
  const float expect[25] = {215, 230, 245, 260, 275, 490, 530, 570, 610,
                            650, 765, 830, 895, 960, 1025, 1040, 1130, 1220,
                            1310, 1400, 1315, 1430, 1545, 1660, 1775};
  const int32_t expect_dims[output_dim_size] = {1, 5, 5};
  Simple(input0, input0_dims, input0_dim_size,
         input1, input1_dims, input1_dim_size,
         output, output_dims, output_dim_size,
         expect, expect_dims);
}

}  // namespace

TEST_F(MatMulOpTest, SimpleCPU) {
  Simple1();
  Simple2();
}

#ifdef MACE_MICRO_ENABLE_CMSIS

namespace {

void TestMatMulQuantInt8(int32_t lhs_rows, int32_t lhs_cols, int32_t rhs_cols) {
  uint32_t input0_size = lhs_rows * lhs_cols;
  uint32_t input1_size = lhs_cols * rhs_cols;
  uint32_t output_size = lhs_rows * rhs_cols;
  float *input0 = new float[input0_size];
  float *input1 = new float[input1_size];
  FillNormalRandomInput(input0, input0_size);
  FillNormalRandomInput(input1, input1_size);
  float *expect_output = new float[output_size];
  const uint32_t MAX_OUTPUT_NUM = 10;
  int32_t *expect_output_dims = new int32_t[MAX_OUTPUT_NUM];

  const int32_t input0_dims[2] = {lhs_rows, lhs_cols};
  // mat0 * tranpose(mat1)
  const int32_t input1_dims[2] = {rhs_cols, lhs_cols};

  MatMulOp matmul_op;
  framework::SubstituteOp substitude_op;
  substitude_op.AddInput(input0, input0_dims, 2)
      .AddInput(input1, input1_dims, 2)
      .AddArg("transpose_a", false)
      .AddArg("transpose_b", true)
      .AddOutput(expect_output, expect_output_dims, MAX_OUTPUT_NUM);
  matmul_op.Init(NULL, reinterpret_cast<framework::OpContext *>(&substitude_op),
                 NULL);
  matmul_op.Run();
  uint32_t expect_output_dim_size = substitude_op.GetOutputShapeDimSize(0);

  int8_t *input0_int8 = new int8_t[input0_size];
  int8_t *input1_int8 = new int8_t[input1_size];
  int8_t *output_int8 = new int8_t[output_size];
  float *output = new float[output_size];
  int32_t *output_dims = new int32_t[MAX_OUTPUT_NUM];
  QuantizeInfo input_quant_info0;
  QuantizeInfo input_quant_info1;
  AutoQuantizeInt8(input0, input0_size, input0_int8, &input_quant_info0.scale,
                   &input_quant_info0.zero);
  AutoQuantizeInt8Symmetric(input1, input1_size, input1_int8,
                            &input_quant_info1.scale);
  QuantizeInfo output_quant_info = {0.0f, 0};
  AdjustRangeInt8(expect_output, output_size, &output_quant_info.scale,
                  &output_quant_info.zero);

  ArmMatMulInt8Op matmul_op_int8;
  framework::SubstituteOp substitude_op_int8;
  substitude_op_int8.AddInput(input0_int8, input0_dims, 2, input_quant_info0)
      .AddInput(input1_int8, input1_dims, 2, input_quant_info1)
      .AddArg("transpose_a", false)
      .AddArg("transpose_b", true)
      .AddOutput(output_int8, output_dims, MAX_OUTPUT_NUM, output_quant_info);
  matmul_op_int8.Init(
      NULL, reinterpret_cast<framework::OpContext *>(&substitude_op_int8),
      NULL);
  matmul_op_int8.Run();
  uint32_t output_dim_size = substitude_op_int8.GetOutputShapeDimSize(0);

  Dequantize(output_int8, output_size, output_quant_info.scale,
             output_quant_info.zero, output);

  ExpectTensorSimilar(expect_output, expect_output_dims, expect_output_dim_size,
                      output, output_dims, output_dim_size, 0.1);

  delete[] input0;
  delete[] input1;
  delete[] expect_output;
  delete[] expect_output_dims;
  delete[] input0_int8;
  delete[] input1_int8;
  delete[] output_int8;
  delete[] output;
  delete[] output_dims;
}

}  // namespace

TEST_F(MatMulOpTest, QuantInt8) {
  TestMatMulQuantInt8(1, 8, 4);
  TestMatMulQuantInt8(1, 1001, 63);
  // WARNING(ZhangZhimin): Batch inputs is unsupported
  // TestMatMulQuantInt8(3, 100, 100);
}

#endif

}  // namespace test
}  // namespace ops
}  // namespace micro
