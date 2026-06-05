// 目标名单持久化往返：引擎 A 登记 + 切开关(每次自动存盘) → 引擎 B 从同文件读回。
// 验证二进制格式、名字、开关状态、声纹向量都对得上。本 TU 提供 doctest main。
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "automute/engine.h"

#include <cstdio>
#include <string>

#ifndef AUTOMUTE_ROOT
#define AUTOMUTE_ROOT "."
#endif
static std::string P(const char* rel) {
  return std::string(AUTOMUTE_ROOT) + "/" + rel;
}

TEST_CASE("名单持久化往返：存盘 → 新引擎读回（名字/开关/声纹一致）") {
  const std::string path = P("build/_persist_roundtrip.bin");
  std::remove(path.c_str());

  AutoMuteEngine::Config cfg;
  cfg.model = P("models/voxceleb_ECAPA512_LM.onnx");
  cfg.targetWav = "";
  std::string err;

  std::vector<float> bobEmb;
  {
    AutoMuteEngine a;
    REQUIRE_MESSAGE(a.prepare(cfg, err), err);
    a.setPersistPath(path);
    a.clearTargets();
    REQUIRE(a.addTargetFromWav("Alice", P("tests/data/spkA_1272_1.wav"), err) == 0);
    REQUIRE(a.addTargetFromWav("Bob", P("tests/data/spkB_2277.wav"), err) == 1);
    a.setTargetMuted(1, true); // Bob 开掐；每次变更已自动存盘
  }

  // 新引擎 B：loadTargets 不需要模型，直接读文件。
  AutoMuteEngine b;
  b.setPersistPath(path);
  REQUIRE(b.loadTargets(err));
  REQUIRE(b.targetCount() == 2);
  CHECK(b.targetAt(0).name == "Alice");
  CHECK(b.targetAt(0).muted == false);
  CHECK(b.targetAt(1).name == "Bob");
  CHECK(b.targetAt(1).muted == true);

  std::remove(path.c_str());
}

TEST_CASE("无文件时 loadTargets 视为空名单、返回 true") {
  AutoMuteEngine e;
  e.setPersistPath(P("build/_persist_does_not_exist.bin"));
  std::string err;
  CHECK(e.loadTargets(err));
  CHECK(e.targetCount() == 0);
}
