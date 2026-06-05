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
  Write-Host "[1/6] 下载 ONNX Runtime 1.26.0 ..."
  $ort = "onnxruntime-win-x64-1.26.0"
  Invoke-WebRequest "https://github.com/microsoft/onnxruntime/releases/download/v1.26.0/$ort.zip" -OutFile third_party\ort.zip
  Expand-Archive third_party\ort.zip -DestinationPath third_party -Force
  Rename-Item third_party\$ort onnxruntime
  Remove-Item third_party\ort.zip
} else { Write-Host "[1/6] ONNX Runtime 已存在，跳过" }

# ---- 2. kaldi-native-fbank（源码，add_subdirectory 编译）----
if (-not (Test-Path third_party\kaldi-native-fbank)) {
  Write-Host "[2/6] 克隆 kaldi-native-fbank ..."
  git clone --depth 1 https://github.com/csukuangfj/kaldi-native-fbank.git third_party\kaldi-native-fbank
} else { Write-Host "[2/6] kaldi-native-fbank 已存在，跳过" }

# ---- 3. dr_wav（单头 wav 读取库）----
if (-not (Test-Path third_party\dr_libs\dr_wav.h)) {
  Write-Host "[3/6] 下载 dr_wav.h ..."
  Invoke-WebRequest "https://raw.githubusercontent.com/mackron/dr_libs/master/dr_wav.h" -OutFile third_party\dr_libs\dr_wav.h
} else { Write-Host "[3/6] dr_wav.h 已存在，跳过" }

# ---- 4. 声纹模型（wespeaker ECAPA-TDNN，VoxCeleb）----
if (-not (Test-Path models\voxceleb_ECAPA512_LM.onnx)) {
  Write-Host "[4/6] 下载声纹模型（约 24MB）..."
  Invoke-WebRequest "https://huggingface.co/Wespeaker/wespeaker-ecapa-tdnn512-LM/resolve/main/voxceleb_ECAPA512_LM.onnx" -OutFile models\voxceleb_ECAPA512_LM.onnx
} else { Write-Host "[4/6] 声纹模型已存在，跳过" }

# ---- 5. webview 0.12.0 单头 + WebView2 SDK 头（M4.4 GUI 外壳）----
#   MinGW 下靠 webview 内置加载器 + 显式链接，【无需 .lib、无需 WebView2Loader.dll】，
#   只要这俩头 + 系统已装 WebView2 运行时（Win10/11 多已预装）。
New-Item -ItemType Directory -Force third_party\webview\include | Out-Null
if (-not (Test-Path third_party\webview\include\webview.h)) {
  Write-Host "[5/6] 下载 webview 0.12.0 单头 ..."
  Invoke-WebRequest "https://raw.githubusercontent.com/webview/webview/0.12.0/core/include/webview/webview.h" -OutFile third_party\webview\include\webview.h
} else { Write-Host "[5/6] webview.h 已存在，跳过" }
if (-not (Test-Path third_party\webview\include\WebView2.h)) {
  Write-Host "[5/6] 下载 WebView2 SDK 头（从 nuget 解出）..."
  Invoke-WebRequest "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/1.0.2792.45" -OutFile third_party\wv2.zip
  Expand-Archive third_party\wv2.zip -DestinationPath third_party\wv2tmp -Force
  Copy-Item third_party\wv2tmp\build\native\include\WebView2.h third_party\webview\include\
  Copy-Item third_party\wv2tmp\build\native\include\WebView2EnvironmentOptions.h third_party\webview\include\
  Remove-Item third_party\wv2.zip; Remove-Item -Recurse -Force third_party\wv2tmp
} else { Write-Host "[5/6] WebView2.h 已存在，跳过" }

# ---- 6. doctest 单头（测试框架，仅 tests/ 用；缺了不影响主程序构建）----
New-Item -ItemType Directory -Force third_party\doctest | Out-Null
if (-not (Test-Path third_party\doctest\doctest.h)) {
  Write-Host "[6/6] 下载 doctest 单头 ..."
  Invoke-WebRequest "https://raw.githubusercontent.com/doctest/doctest/v2.4.11/doctest/doctest.h" -OutFile third_party\doctest\doctest.h
} else { Write-Host "[6/6] doctest.h 已存在，跳过" }

Write-Host ""
Write-Host "✅ 依赖就绪。现在可以： cmake -S . -B build -G Ninja && cmake --build build"
