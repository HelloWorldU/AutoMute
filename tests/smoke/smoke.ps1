# 冒烟测试（Tier 3）：构建 + 单元/集成测试 + 可执行启动不崩。
# 防"改崩了构建/启动"。真机掐声/路由/拖拽缩放是人工清单（docs/testing.md）。
#   用法:  powershell -ExecutionPolicy Bypass -File tests\smoke\smoke.ps1
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Set-Location $root
$fail = 0

Write-Host "== 1. 构建全部目标 =="
cmake --build build
if ($LASTEXITCODE -ne 0) { Write-Host "[FAIL] 构建失败"; exit 1 }

Write-Host "`n== 2. ctest（单元 + 集成） =="
ctest --test-dir build --output-on-failure
if ($LASTEXITCODE -ne 0) { $fail++ }

Write-Host "`n== 3. CLI --apps 退出码 0 =="
& .\build\bin\automute_app.exe --apps | Out-Null
if ($LASTEXITCODE -ne 0) { Write-Host "[FAIL] automute_app --apps 退出码 $LASTEXITCODE"; $fail++ }
else { Write-Host "[OK]" }

Write-Host "`n== 4. GUI 起窗口存活 3s（不崩） =="
$p = Start-Process -FilePath ".\build\bin\automute_gui.exe" -PassThru
Start-Sleep -Seconds 3
if ($p.HasExited) { Write-Host "[FAIL] GUI 提前退出 $($p.ExitCode)"; $fail++ }
else { Write-Host "[OK] 存活"; Stop-Process -Id $p.Id -Force }

Write-Host ""
if ($fail -eq 0) { Write-Host "[PASS] smoke all passed" }
else { Write-Host "[FAIL] $fail check(s) failed"; exit 1 }
