#pragma once
//
// 读 wav 文件 → 单声道 float 样本（多声道自动混成单声道）。
// 声纹录入/测试都要从 wav 拿音频，所以单独成一个可复用模块。
//
#include <cstdint>
#include <string>
#include <vector>

// 成功返回 true，out 填样本、sampleRate 填采样率（不重采样，原样返回）。
// 失败返回 false，err 填错误信息。
bool loadWavMono(const std::string& path, std::vector<float>& out,
                 uint32_t& sampleRate, std::string& err);
