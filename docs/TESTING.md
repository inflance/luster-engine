# Luster 自动化测试指南

本文档描述了如何使用 GitHub CLI 进行 Luster 项目的自动化测试。

## 快速开始

### 1. 安装 GitHub CLI

Windows:
```powershell
winget install --id GitHub.cli
```

macOS:
```bash
brew install gh
```

Linux:
```bash
sudo apt install gh  # Ubuntu/Debian
sudo yum install gh  # CentOS/RHEL
```

### 2. 认证和设置

```powershell
# 登录 GitHub
gh auth login

# 验证认证状态
gh auth status

# 设置仓库
cd luster
gh repo view
```

### 3. 运行测试

#### 使用 PowerShell 脚本

```powershell
# 运行构建测试
.github\scripts\gh-test.ps1 -Mode build -Preset vs2022

# 运行所有测试
.github\scripts\gh-test.ps1 -Mode all -Watch

# 查看测试状态
.github\scripts\gh-test.ps1 -Mode status

# 生成本地测试报告
.github\scripts\gh-report.ps1 -Format html -IncludeLogs
```

#### 使用 GitHub CLI 直接调用

```bash
# 触发构建工作流
gh workflow run build.yml --ref main --field preset=vs2022

# 触发测试工作流
gh workflow run test.yml --ref main

# 查看运行状态
gh run list --limit=5

# 查看详细日志
gh run view --log
```

## 测试类型

### 1. 构建测试 (build.yml)
- **目标**: 验证所有配置能成功构建
- **触发**: Push/PR 到 main/develop 分支
- **配置**:
  - Windows 平台
  - Visual Studio 2022 / Ninja
  - Debug/Release 模式
  - 有/无 vcpkg 选项

### 2. 功能测试 (test.yml)
- **目标**: 运行时功能验证
- **触发**: 构建测试成功后
- **测试内容**:
  - Vulkan 初始化测试
  - 窗口创建测试
  - 内存泄漏检测
  - 兼容性测试

### 3. 本地测试

```powershell
# 本地构建测试
cmake --preset vs2022
cmake --build --preset vs2022-Release --parallel

# 本地运行测试
out/build/vs2022/bin/Release/luster_sandbox.exe --test-runtime

# 运行单元测试
out/bin/tests/luster_tests.exe
```

## 配置说明

### GitHub Actions 工作流

| 文件 | 描述 |
|------|------|
| `.github/workflows/build.yml` | 构建验证工作流 |
| `.github/workflows/test.yml` | 运行时测试工作流 |
| `.github/scripts/gh-test.ps1` | 本地测试脚本 |
| `.github/scripts/gh-report.ps1` | 报告生成脚本 |

### 环境变量

| 变量 | 描述 | 示例 |
|------|------|------|
| `VULKAN_SDK` | Vulkan SDK 路径 | `C:\VulkanSDK\1.3.268.0` |
| `SDL_VIDEODRIVER` | SDL 视频驱动 | `windows` |

## 故障排除

### 常见问题

1. **GitHub CLI 未安装**
   ```powershell
   # 检查安装
   gh --version
   
   # 重新安装
   winget install --id GitHub.cli
   ```

2. **认证失败**
   ```powershell
   # 重新认证
   gh auth login
   
   # 检查认证状态
   gh auth status
   ```

3. **构建失败**
   ```powershell
   # 检查日志
   gh run view --log --job=<job-id>
   
   # 本地重现
   cmake --preset vs2022
   cmake --build --preset vs2022-Debug --parallel
   ```

4. **Vulkan 测试失败**
   ```powershell
   # 检查 Vulkan SDK
   echo $env:VULKAN_SDK
   
   # 验证安装
   vulkaninfo
   ```

## 高级用法

### 自定义测试配置

创建 `.github/workflows/custom-test.yml`:

```yaml
name: Custom Test
on:
  workflow_dispatch:
    inputs:
      preset:
        description: 'CMake preset'
        required: true
        default: 'vs2022'
        type: choice
        options:
          - vs2022
          - vs2022-vcpkg
          - ninja-multi

jobs:
  custom-test:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
      - name: Setup and Test
        run: |
          cmake --preset ${{ inputs.preset }}
          cmake --build --preset ${{ inputs.preset }}-Release --parallel
```

### 性能测试

```powershell
# 运行性能测试
.github\scripts\gh-test.ps1 -Mode build -Preset vs2022
# 工作流中添加性能测试步骤
```

## 报告和分析

### 生成测试报告

```powershell
# 获取最新运行 ID
$runId = gh run list --limit=1 -q '.[0].databaseId'

# 生成 HTML 报告
.github\scripts\gh-report.ps1 -RunId $runId -Format html -IncludeLogs

# 生成 JSON 报告用于自动化处理
.github\scripts\gh-report.ps1 -Format json
```

### 监控测试趋势

```powershell
# 查看历史趋势
gh run list --workflow=build.yml --limit=10

# 分析失败模式
gh run view --json jobs --jq '.jobs[] | {name: .name, status: .conclusion, started: .startedAt}'
```

## 集成开发环境

### VS Code 配置

在 `.vscode/settings.json` 中添加:

```json
{
    "cmake.configureOnOpen": true,
    "cmake.generator": "Visual Studio 17 2022",
    "cmake.buildDirectory": "${workspaceFolder}/out/build/${buildKit}",
    "cmake.testEnvironment": {
        "PATH": "${workspaceFolder}/out/build/vs2022/bin/Release;${env:PATH}"
    }
}
```

### 调试配置

在 `.vscode/launch.json` 中添加:

```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Luster Debug",
            "type": "cppvsdbg",
            "request": "launch",
            "program": "${workspaceFolder}/out/build/vs2022/bin/Debug/luster_sandbox.exe",
            "args": ["--test-runtime"],
            "stopAtEntry": false,
            "cwd": "${workspaceFolder}/out/build/vs2022/bin/Debug",
            "environment": []
        }
    ]
}
```

## 扩展阅读

- [GitHub CLI 文档](https://cli.github.com/manual/)
- [GitHub Actions 工作流语法](https://docs.github.com/en/actions/using-workflows/workflow-syntax-for-github-actions)
- [GoogleTest 文档](https://google.github.io/googletest/)
- [Vulkan 开发指南](https://vulkan.lunarg.com/doc/sdk/latest/windows/layer_config.html)