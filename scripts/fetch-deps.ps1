# AutoMute 第三方依赖一键拉取脚本（Windows / PowerShell）
#
#   这些依赖体积大或可随时重新获取，不入版本库。clone 仓库后先跑这个脚本，
#   再 cmake 配置/构建。脚本幂等：已存在的不会重复下载。
#
#   用法（在项目根或任意位置）：  powershell -ExecutionPolicy Bypass -File scripts\fetch-deps.ps1
#
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot   # scripts/ 的上一级 = 项目根
Set-Location $root

New-Item -ItemType Directory -Force third_party, third_party\dr_libs, models | Out-Null

# ---- 1. ONNX Runtime 1.26.0（预编译二进制：DLL + 头 + 导入库）----
if (-not (Test-Path third_party\onnxruntime)) {
  Write-Host "[1/4] 下载 ONNX Runtime 1.26.0 ..."
  $ort = "onnxruntime-win-x64-1.26.0"
  Invoke-WebRequest "https://github.com/microsoft/onnxruntime/releases/download/v1.26.0/$ort.zip" -OutFile third_party\ort.zip
  Expand-Archive third_party\ort.zip -DestinationPath third_party -Force
  Rename-Item third_party\$ort onnxruntime
  Remove-Item third_party\ort.zip
} else { Write-Host "[1/4] ONNX Runtime 已存在，跳过" }

# ---- 2. kaldi-native-fbank（源码，add_subdirectory 编译）----
if (-not (Test-Path third_party\kaldi-native-fbank)) {
  Write-Host "[2/4] 克隆 kaldi-native-fbank ..."
  git clone --depth 1 https://github.com/csukuangfj/kaldi-native-fbank.git third_party\kaldi-native-fbank
} else { Write-Host "[2/4] kaldi-native-fbank 已存在，跳过" }

# ---- 3. dr_wav（单头 wav 读取库）----
if (-not (Test-Path third_party\dr_libs\dr_wav.h)) {
  Write-Host "[3/4] 下载 dr_wav.h ..."
  Invoke-WebRequest "https://raw.githubusercontent.com/mackron/dr_libs/master/dr_wav.h" -OutFile third_party\dr_libs\dr_wav.h
} else { Write-Host "[3/4] dr_wav.h 已存在，跳过" }

# ---- 4. 声纹模型（wespeaker ECAPA-TDNN，VoxCeleb）----
if (-not (Test-Path models\voxceleb_ECAPA512_LM.onnx)) {
  Write-Host "[4/4] 下载声纹模型（约 24MB）..."
  Invoke-WebRequest "https://huggingface.co/Wespeaker/wespeaker-ecapa-tdnn512-LM/resolve/main/voxceleb_ECAPA512_LM.onnx" -OutFile models\voxceleb_ECAPA512_LM.onnx
} else { Write-Host "[4/4] 声纹模型已存在，跳过" }

Write-Host ""
Write-Host "✅ 依赖就绪。现在可以： cmake -S . -B build -G Ninja && cmake --build build"
