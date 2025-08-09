# CI 工作流验证脚本
# 完整验证 Luster 项目的 CI/CD 配置

param(
    [ValidateSet("local", "remote", "full")]
    [string]$Mode = "full",
    
    [switch]$Interactive,
    [switch]$Verbose
)

# 颜色定义
$Colors = @{
    Success = "Green"
    Warning = "Yellow"
    Error = "Red"
    Info = "Cyan"
    Debug = "Gray"
}

# 验证状态
$ValidationResults = @{
    LocalBuild = $false
    LocalTest = $false
    RemoteBuild = $false
    RemoteTest = $false
    Dependencies = $false
    Configuration = $false
}

function Write-Status {
    param([string]$Message, [string]$Status = "Info")
    Write-Host $Message -ForegroundColor $Colors[$Status]
}

function Test-Command {
    param([string]$Command, [string]$Description)
    Write-Status "验证: $Description" "Info"
    try {
        Invoke-Expression $Command 2>$null
        return $true
    } catch {
        Write-Status "失败: $Description" "Error"
        return $false
    }
}

# 1. 本地构建验证
function Test-LocalBuild {
    Write-Status "`n🛠️  本地构建验证开始..." "Info"
    
    $tests = @(
        @{
            Name = "CMake 配置"
            Command = "cmake --preset vs2022"
            Success = $false
        },
        @{
            Name = "Release 构建"
            Command = "cmake --build --preset vs2022-Release --parallel"
            Success = $false
        },
        @{
            Name = "Debug 构建"
            Command = "cmake --build --preset vs2022-Debug --parallel"
            Success = $false
        },
        @{
            Name = "测试构建"
            Command = "cmake --build --preset vs2022-Release --target luster_tests"
            Success = $false
        }
    )
    
    foreach ($test in $tests) {
        Write-Status "测试: $($test.Name)" "Debug"
        $result = Test-Command $test.Command $test.Name
        $test.Success = $result
        if ($result) {
            Write-Status "✅ $($test.Name) 成功" "Success"
        } else {
            Write-Status "❌ $($test.Name) 失败" "Error"
            return $false
        }
    }
    
    return $true
}

# 2. 本地测试验证
function Test-LocalTest {
    Write-Status "`n🧪 本地测试验证开始..." "Info"
    
    $testPath = "out/build/vs2022/bin/tests/Release/luster_tests.exe"
    if (Test-Path $testPath) {
        $testResult = Test-Command "$testPath --gtest_output=xml:test_results.xml" "Google Test 运行"
        if ($testResult) {
            Write-Status "✅ 本地测试通过" "Success"
            return $true
        }
    } else {
        Write-Status "❌ 测试可执行文件未找到: $testPath" "Error"
        return $false
    }
}

# 3. 依赖验证
function Test-Dependencies {
    Write-Status "`n📦 依赖验证开始..." "Info"
    
    $deps = @(
        @{ Name = "GitHub CLI"; Command = "gh --version" },
        @{ Name = "CMake"; Command = "cmake --version" },
        @{ Name = "Git"; Command = "git --version" }
    )
    
    $allGood = $true
    foreach ($dep in $deps) {
        if (Test-Command $dep.Command $dep.Name) {
            Write-Status "✅ $($dep.Name) 已安装" "Success"
        } else {
            Write-Status "❌ $($dep.Name) 未安装" "Error"
            $allGood = $false
        }
    }
    
    return $allGood
}

# 4. 配置验证
function Test-Configuration {
    Write-Status "`n⚙️  配置验证开始..." "Info"
    
    $checks = @(
        @{ Name = "GitHub 仓库"; Test = { git remote get-url origin } },
        @{ Name = "工作流文件"; Test = { Test-Path ".github/workflows/build.yml" } },
        @{ Name = "测试配置"; Test = { Test-Path "tests/CMakeLists.txt" } },
        @{ Name = "GitHub CLI 认证"; Test = { gh auth status 2>$null } }
    )
    
    $allGood = $true
    foreach ($check in $checks) {
        try {
            $result = & $check.Test
            Write-Status "✅ $($check.Name) 正常" "Success"
        } catch {
            Write-Status "❌ $($check.Name) 异常" "Error"
            $allGood = $false
        }
    }
    
    return $allGood
}

# 5. 远程验证
function Test-RemoteBuild {
    Write-Status "`n🌐 远程构建验证开始..." "Info"
    
    if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
        Write-Status "GitHub CLI 未安装，跳过远程验证" "Warning"
        return $false
    }
    
    # 检查认证状态
    $authStatus = gh auth status 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Status "GitHub 未认证，请先运行: gh auth login" "Error"
        return $false
    }
    
    # 触发远程构建
    Write-Status "触发远程构建..." "Info"
    $trigger = gh workflow run build.yml --ref $(git rev-parse --abbrev-ref HEAD) 2>&1
    
    if ($LASTEXITCODE -eq 0) {
        Write-Status "✅ 远程构建已触发" "Success"
        
        # 等待并监视
        Start-Sleep 5
        $runId = gh run list --workflow=build.yml --limit=1 --json databaseId -q '.[0].databaseId'
        if ($runId) {
            Write-Status "构建运行ID: $runId" "Debug"
            return $true
        }
    } else {
        Write-Status "❌ 远程构建触发失败: $trigger" "Error"
    }
    
    return $false
}

# 6. 生成验证报告
function New-ValidationReport {
    Write-Status "`n📋 生成验证报告..." "Info"
    
    $report = @"
# CI 工作流验证报告
生成时间: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")

## 验证结果
- 本地构建: $($ValidationResults.LocalBuild ? "✅ 通过" : "❌ 失败")
- 本地测试: $($ValidationResults.LocalTest ? "✅ 通过" : "❌ 失败")
- 依赖检查: $($ValidationResults.Dependencies ? "✅ 通过" : "❌ 失败")
- 配置检查: $($ValidationResults.Configuration ? "✅ 通过" : "❌ 失败")
- 远程构建: $($ValidationResults.RemoteBuild ? "✅ 通过" : "❌ 失败")

## 环境信息
- 操作系统: $(Get-CimInstance Win32_OperatingSystem | Select-Object -ExpandProperty Caption)
- CMake 版本: $(cmake --version -split "`n")[0]
- Git 版本: $(git --version)
- GitHub CLI 版本: $(gh --version -split "`n")[0]

## 建议操作
$(
    if (-not $ValidationResults.Dependencies) {
        "1. 安装缺失的依赖项"
    }
    if (-not $ValidationResults.LocalBuild) {
        "2. 修复本地构建问题"
    }
    if (-not $ValidationResults.LocalTest) {
        "3. 修复测试问题"
    }
    if (-not $ValidationResults.RemoteBuild) {
        "4. 配置 GitHub CLI 认证"
    }
)

## 下一步
1. 修复所有 ❌ 标记的问题
2. 重新运行验证
3. 提交代码触发 CI
"@
    
    $report | Out-File -FilePath "ci-validation-report.md" -Encoding UTF8
    Write-Status "报告已保存到 ci-validation-report.md" "Success"
}

# 主验证流程
function Start-Validation {
    Write-Status "🚀 开始 CI 工作流验证..." "Info"
    
    # 清理之前的构建
    if (Test-Path "out/build/vs2022") {
        Remove-Item -Recurse -Force "out/build/vs2022"
    }
    
    # 执行验证
    switch ($Mode) {
        "local" {
            $ValidationResults.LocalBuild = Test-LocalBuild
            $ValidationResults.LocalTest = Test-LocalTest
            $ValidationResults.Dependencies = Test-Dependencies
            $ValidationResults.Configuration = Test-Configuration
        }
        "remote" {
            $ValidationResults.RemoteBuild = Test-RemoteBuild
        }
        "full" {
            $ValidationResults.LocalBuild = Test-LocalBuild
            $ValidationResults.LocalTest = Test-LocalTest
            $ValidationResults.Dependencies = Test-Dependencies
            $ValidationResults.Configuration = Test-Configuration
            $ValidationResults.RemoteBuild = Test-RemoteBuild
        }
    }
    
    # 生成报告
    New-ValidationReport
    
    # 总结
    $passed = ($ValidationResults.Values | Where-Object { $_ -eq $true }).Count
    $total = $ValidationResults.Values.Count
    
    Write-Status "`n📊 验证完成: $passed/$total 项通过" ($passed -eq $total ? "Success" : "Warning")
    
    return $ValidationResults
}

# 交互模式
if ($Interactive) {
    Write-Status "🎯 交互验证模式" "Info"
    
    $choices = @(
        "1. 本地验证",
        "2. 远程验证",
        "3. 完整验证",
        "4. 退出"
    )
    
    $choice = Read-Host "`n请选择验证模式:`n$($choices -join "`n")`n"
    
    switch ($choice) {
        "1" { $Mode = "local" }
        "2" { $Mode = "remote" }
        "3" { $Mode = "full" }
        "4" { exit }
    }
}

# 执行验证
return Start-Validation