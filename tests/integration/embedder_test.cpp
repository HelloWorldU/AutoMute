// 声纹管线集成测试：wav → fbank → ONNX → embedding → 余弦。离线确定可复现。
// 用 models/test_speakers 的样本断言"同人高、异人低"。本 TU 提供 doctest main。
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "automute/audio/embedder.h"

#include <string>
#include <vector>

#ifndef AUTOMUTE_ROOT
#define AUTOMUTE_ROOT "."
#endif
static std::string P(const char* rel) {
  return std::string(AUTOMUTE_ROOT) + "/" + rel;
}

TEST_CASE("cosineSimilarity 基本性质") {
  CHECK(cosineSimilarity({1, 0, 0}, {1, 0, 0}) == doctest::Approx(1.0));
  CHECK(cosineSimilarity({1, 0, 0}, {0, 1, 0}) == doctest::Approx(0.0));
  CHECK(cosineSimilarity({1, 0}, {-1, 0}) == doctest::Approx(-1.0));
  CHECK(cosineSimilarity({0, 0, 0}, {1, 2, 3}) == doctest::Approx(0.0));
  CHECK(cosineSimilarity({1, 2, 3}, {2, 4, 6}) == doctest::Approx(1.0));
}

TEST_CASE("声纹管线：同人相似度高、异人低、区分度大") {
  Embedder emb;
  std::string err;
  REQUIRE_MESSAGE(emb.load(P("models/voxceleb_ECAPA512_LM.onnx"), err), err);

  std::vector<float> a1, a2, b;
  REQUIRE(emb.embedWav(P("models/test_speakers/spkA_1272_1.wav"), a1, err));
  REQUIRE(emb.embedWav(P("models/test_speakers/spkA_1272_2.wav"), a2, err));
  REQUIRE(emb.embedWav(P("models/test_speakers/spkB_2277.wav"), b, err));

  CHECK(a1.size() == 192);
  float same = cosineSimilarity(a1, a2); // 同人，实测 ~0.8
  float diff = cosineSimilarity(a1, b);  // 异人，实测 ~0.1
  MESSAGE("同人相似度 = ", same, " / 异人相似度 = ", diff);
  CHECK(same > 0.6f);
  CHECK(diff < 0.35f);
  CHECK(same > diff + 0.3f);
}
