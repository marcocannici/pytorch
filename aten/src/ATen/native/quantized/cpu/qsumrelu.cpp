#include <ATen/ATen.h>
#include <ATen/core/Type.h>
#include <ATen/core/op_registration/op_registration.h>
#include <ATen/native/TensorIterator.h>
#include <ATen/native/cpu/Loops.h>
#include <ATen/quantized/Quantizer.h>
#include <fbgemm/QuantUtils.h>

#include <algorithm>

namespace at { namespace native {
namespace {
inline int32_t clamp(int32_t val, int32_t lo, int32_t hi) {
  return std::min(hi, std::max(lo, val));
}

class QSumReLUInt8 final : public c10::OperatorKernel {
 public:
  Tensor operator()(at::Tensor qa, at::Tensor qb,
                     double scale, int64_t zero_point) {
    AT_ASSERTM(qa.numel() == qb.numel(), "Sum operands must be the same size!");
    auto a = qa.dequantize();
    auto b = qb.dequantize();
    auto c = at::empty_like(a);
    auto iter = TensorIterator::binary_op(c, a, b);
    binary_kernel(*iter, [&](float a_val, float b_val) -> float {
      return std::max<float>(a_val + b_val, 0);
    });
    return c.quantize_linear(scale, zero_point);  // Requantize
  }
};

static auto registry = c10::RegisterOperators().op(
    "quantized::sum_relu(Tensor qa, Tensor qb, float scale, int zero_point)"
      "-> Tensor",
    c10::kernel<QSumReLUInt8>(),
    c10::dispatchKey(QuantizedCPUTensorId()));
}  // namespace
}}  // namespace at::native
