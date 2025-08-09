# Luster

基于 Vulkan + SDL3 的最小游戏引擎脚手架，第三方库以 Git submodule + CMake 源码构建。示例程序见 [src/main.cpp](src/main.cpp)。

## 技术栈
- 窗口/输入：SDL3（静态库）
- Vulkan 加载器：volk
- 数学库：glm（头文件）
- 日志：spdlog
- Vulkan 头：Vulkan-Headers

## 目录结构
```text
.
├─ CMakeLists.txt
├─ CMakePresets.json
├─ external/
│  ├─ SDL/                 # SDL3 源码
│  ├─ volk/                # Vulkan loader
│  ├─ glm/                 # 头文件
│  ├─ spdlog/              # 日志
│  └─ Vulkan-Headers/      # <vulkan/vulkan.h>
├─ src/
│  ├─ CMakeLists.txt
│  └─ main.cpp             # SDL3+Vulkan 最小样例
└─ out/                    # 生成目录（由 CMakePresets 定义）
```

## 先决条件
- Windows 10/11 x64，Visual Studio 2022（含 MSVC 工具链）
- CMake ≥ 3.21，Git
- Vulkan Runtime/驱动（建议安装显卡驱动附带的 Vulkan Runtime）

## 获取代码与子模块
```bash
git clone <your-repo> Luster
cd Luster
git submodule update --init --recursive
```

## 配置与构建
使用 CMake 预设（默认不启用 vcpkg）：
```bash
cmake --preset vs2022
cmake --build --preset vs2022-Debug --parallel
```

运行示例：
```bash
out/build/vs2022/bin/Debug/Luster_sandbox.exe
```

切换 Release：
```bash
cmake --build --preset vs2022-Release --parallel
```

启用 vcpkg（可选）：
```bash
cmake --preset vs2022-vcpkg
cmake --build --preset vs2022-vcpkg-Debug --parallel
```

详情见工程文件：
- 顶层构建脚本：[CMakeLists.txt](CMakeLists.txt)
- 第三方集成：[external/CMakeLists.txt](external/CMakeLists.txt)
- 示例入口：[src/main.cpp](src/main.cpp)
- 预设配置：[CMakePresets.json](CMakePresets.json)

## 常见问题
- 无法初始化 SDL（unknown）：确保在桌面环境下运行，并已加载 Windows 视频驱动。可设置环境变量强制驱动：
```bash
set SDL_VIDEODRIVER=windows
```
工程中已在 [src/main.cpp](src/main.cpp) 通过 SDL hint/注册入口进行处理。

- 找不到 Vulkan 设备/扩展：安装最新版 GPU 驱动与 Vulkan Runtime。

## 后续规划
- 设备/队列选择、Swapchain 管线
- 命令缓冲/同步原语封装
- 资源管理与渲染框架

## 许可证
TBD