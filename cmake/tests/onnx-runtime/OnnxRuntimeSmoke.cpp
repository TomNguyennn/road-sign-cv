#include <onnxruntime_cxx_api.h>

#include <array>
#include <iostream>
#include <string_view>

int main() {
    try {
        if (std::string_view{OrtGetApiBase()->GetVersionString()} != "1.22.0") {
            std::cerr << "Unexpected ONNX Runtime version\n";
            return 1;
        }
        Ort::Env environment{ORT_LOGGING_LEVEL_WARNING, "dependency-smoke"};
        auto memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        std::array<float, 2> values{1.0F, 2.0F};
        const std::array<int64_t, 2> shape{1, 2};
        auto tensor = Ort::Value::CreateTensor<float>(
            memoryInfo, values.data(), values.size(), shape.data(), shape.size());
        if (!tensor.IsTensor() || tensor.GetTensorTypeAndShapeInfo().GetElementCount() != 2 ||
            tensor.GetTensorData<float>()[1] != 2.0F) {
            std::cerr << "Native tensor round trip failed\n";
            return 1;
        }
        std::cout << "ONNX Runtime 1.22.0: native C++ API and tensor round trip passed\n";
        return 0;
    } catch (const Ort::Exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
