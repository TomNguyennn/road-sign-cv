#include <onnxruntime_cxx_api.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <iostream>
#include <string_view>

namespace {
constexpr std::array<float, 2> TENSOR_VALUES{1.0F, 2.0F};
constexpr std::array<int64_t, 2> TENSOR_SHAPE{1, 2};
constexpr std::size_t EXPECTED_ELEMENT_COUNT = 2;
} // namespace

int main() {
    try {
        if (std::string_view{OrtGetApiBase()->GetVersionString()} != "1.22.0") {
            std::cerr << "Unexpected ONNX Runtime version\n";
            return 1;
        }
        Ort::Env environment{ORT_LOGGING_LEVEL_WARNING, "dependency-smoke"};
        auto memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        auto values = TENSOR_VALUES;
        auto tensor = Ort::Value::CreateTensor<float>(memoryInfo, values.data(), values.size(),
                                                      TENSOR_SHAPE.data(), TENSOR_SHAPE.size());
        std::array<float, 2> tensorValues{};
        tensor.GetTensorMutableData<float>();
        std::copy_n(tensor.GetTensorData<float>(), tensorValues.size(), tensorValues.begin());
        if (!tensor.IsTensor() ||
            tensor.GetTensorTypeAndShapeInfo().GetElementCount() != EXPECTED_ELEMENT_COUNT ||
            tensorValues[1] != TENSOR_VALUES[1]) {
            std::cerr << "Native tensor round trip failed\n";
            return 1;
        }
        std::cout << "ONNX Runtime 1.22.0: native C++ API and tensor round trip passed\n";
        return 0;
    } catch (const Ort::Exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
