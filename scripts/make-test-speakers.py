#!/usr/bin/env python
# 生成 M2.3 声纹验证用的测试样本（同人×2 + 异人×1，16kHz 单声道）。
#
#   依赖: python -m pip install datasets soundfile
#   用法: python scripts/make-test-speakers.py
#   输出: models/test_speakers/{spkA_1272_1, spkA_1272_2, spkB_2277}.wav
#
# 数据来源：LibriSpeech（公开数据集）。spkA 取自 dummy 的 speaker 1272（两句），
# spkB 流式从完整 dev-clean 取第一个不同说话人（只读首块，不全量下载）。
import io, os
import soundfile as sf
from datasets import load_dataset, Audio

os.makedirs("models/test_speakers", exist_ok=True)

# 同一人两段
dsA = load_dataset("hf-internal-testing/librispeech_asr_dummy", "clean",
                   split="validation").cast_column("audio", Audio(decode=False))
spkA = None
for i in range(2):
    r = dsA[i]
    data, sr = sf.read(io.BytesIO(r["audio"]["bytes"]))
    if data.ndim > 1:
        data = data.mean(axis=1)
    spkA = r["speaker_id"]
    sf.write(f"models/test_speakers/spkA_{spkA}_{i+1}.wav", data, sr,
             subtype="PCM_16")

# 不同的人一段（流式）
dsB = load_dataset("openslr/librispeech_asr", "clean", split="validation",
                   streaming=True).cast_column("audio", Audio(decode=False))
for r in dsB:
    if r["speaker_id"] != spkA:
        data, sr = sf.read(io.BytesIO(r["audio"]["bytes"]))
        if data.ndim > 1:
            data = data.mean(axis=1)
        sf.write(f"models/test_speakers/spkB_{r['speaker_id']}.wav", data, sr,
                 subtype="PCM_16")
        break

print("测试样本已生成到 models/test_speakers/")

# 额外生成 48kHz 立体声版（模拟 WASAPI 抓到的格式），用于验证重采样链路。
import glob, numpy as np
from scipy.signal import resample_poly
os.makedirs("models/test_speakers_48k", exist_ok=True)
for f in glob.glob("models/test_speakers/*.wav"):
    data, sr = sf.read(f)
    if data.ndim > 1:
        data = data.mean(axis=1)
    up = resample_poly(data, 48000, sr)
    stereo = np.stack([up, up], axis=1)
    sf.write("models/test_speakers_48k/" + os.path.basename(f), stereo, 48000,
             subtype="PCM_16")
print("48k 立体声版已生成到 models/test_speakers_48k/")
