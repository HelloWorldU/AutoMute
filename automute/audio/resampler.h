#pragma once
//
// 重采样到 16kHz（声纹模型要求）。
//   48000 → 16000：抗混叠 FIR 低通 + 抽取 3（WASAPI 默认就是 48k，主路径）
//   16000：直通
//   其它：线性插值（够用的退路）
//
#include <cstdint>
#include <vector>

void resampleTo16k(const std::vector<float>& in, uint32_t inRate,
                   std::vector<float>& out);
