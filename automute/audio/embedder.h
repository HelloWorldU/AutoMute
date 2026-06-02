#pragma once
//
// 声纹提取器：wav/样本 → 192 维 embedding。
// 把"读音频 → fbank → 模型推理"封装成可复用单元，录入与实时分析都用它。
//
#include <cstdint>
#include <string>
#include <vector>

class Embedder {
public:
  Embedder();
  ~Embedder();

  // 加载 ONNX 声纹模型。失败返回 false 并填 err。
  bool load(const std::string& modelPath, std::string& err);

  // 从单声道样本算声纹（内部：fbank + 均值归一化 + 推理）。
  bool embed(const std::vector<float>& mono, uint32_t sampleRate,
             std::vector<float>& out, std::string& err);

  // 便捷：直接从 wav 文件算声纹。
  bool embedWav(const std::string& wavPath, std::vector<float>& out,
                std::string& err);

private:
  struct Impl;
  Impl* impl_ = nullptr;
};

// 两个声纹的余弦相似度（[-1,1]，越接近 1 越像同一个人）。
float cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b);
