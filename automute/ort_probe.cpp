//
// M2.1 探针：验证 ONNX Runtime 能在本项目（MinGW + CMake）下链接并初始化。
// 不做真正推理（还没模型）——能创建 Env、打印版本，就证明工具链可用。
//
#include <cstdio>

#include "onnxruntime_cxx_api.h"

int main() {
  printf("ONNX Runtime 版本: %s\n", Ort::GetVersionString().c_str());

  // 创建 Env 会真正初始化 ORT 运行时——这一步不崩，链接就算通了。
  Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "automute");
  printf("Env 创建成功 —— ORT 工具链在 MinGW 下可用。\n");
  return 0;
}
