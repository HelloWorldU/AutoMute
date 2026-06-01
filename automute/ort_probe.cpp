//
// M2.1 + M2.2a 探针：
//   M2.1  —— 验证 ONNX Runtime 在 MinGW 下能链接、能初始化。
//   M2.2a —— 加载真实声纹模型，打印输入/输出形状，用【假数据】跑一次推理，
//            确认能吐出 embedding 向量（绕开真实特征提取，先证明推理 API 会用）。
//
#include <cstdio>
#include <string>
#include <vector>

#include "onnxruntime_cxx_api.h"

// ORT 在 Windows 上要 wchar_t 路径；模型路径是 ASCII，简单加宽即可。
static std::wstring widen(const std::string& s) {
  return std::wstring(s.begin(), s.end());
}

static void printShape(const std::vector<int64_t>& shape) {
  printf("[");
  for (auto d : shape)
    printf("%lld ", (long long)d);
  printf("]");
}

int main(int argc, char** argv) {
  std::string model =
      (argc > 1) ? argv[1] : "models/voxceleb_ECAPA512_LM.onnx";

  printf("ONNX Runtime 版本: %s\n", Ort::GetVersionString().c_str());

  Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "automute");
  Ort::SessionOptions opts;
  Ort::Session session(env, widen(model).c_str(), opts);
  Ort::AllocatorWithDefaultOptions alloc;

  // ---- 枚举输入/输出，打印名字与形状（-1 表示动态维）----
  std::vector<std::string> inNames, outNames;
  std::vector<int64_t> inShape;

  printf("\n--- 输入 ---\n");
  for (size_t i = 0; i < session.GetInputCount(); ++i) {
    auto name = session.GetInputNameAllocated(i, alloc);
    inNames.push_back(name.get());
    auto shape =
        session.GetInputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape();
    if (i == 0)
      inShape = shape;
    printf("  [%zu] %-10s shape=", i, name.get());
    printShape(shape);
    printf("\n");
  }

  printf("--- 输出 ---\n");
  for (size_t i = 0; i < session.GetOutputCount(); ++i) {
    auto name = session.GetOutputNameAllocated(i, alloc);
    outNames.push_back(name.get());
    auto shape =
        session.GetOutputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape();
    printf("  [%zu] %-10s shape=", i, name.get());
    printShape(shape);
    printf("\n");
  }

  // ---- 构造假输入：动态维(-1)填上：第0维=1(batch)，其余=200(帧)，末维保持(80)----
  std::vector<int64_t> dummyShape = inShape;
  for (size_t i = 0; i < dummyShape.size(); ++i)
    if (dummyShape[i] < 0)
      dummyShape[i] = (i == 0) ? 1 : 200;

  size_t count = 1;
  for (auto d : dummyShape)
    count *= (size_t)d;
  std::vector<float> dummy(count, 0.0f); // 全 0 假数据

  auto memInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
  Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
      memInfo, dummy.data(), dummy.size(), dummyShape.data(),
      dummyShape.size());

  const char* inName = inNames[0].c_str();
  std::vector<const char*> outPtrs;
  for (auto& n : outNames)
    outPtrs.push_back(n.c_str());

  printf("\n用假输入 shape=");
  printShape(dummyShape);
  printf(" 跑一次推理...\n");

  auto outputs = session.Run(Ort::RunOptions{nullptr}, &inName, &inputTensor, 1,
                             outPtrs.data(), outPtrs.size());

  auto outShape = outputs[0].GetTensorTypeAndShapeInfo().GetShape();
  printf("✅ 推理成功！输出 embedding shape=");
  printShape(outShape);
  printf("\n");
  return 0;
}
