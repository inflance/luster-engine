# C++ 代码风格指南

适用于本仓库的 C++/CMake 项目（SDL3、GLM 等第三方库）。目标是提升可读性、一致性与可维护性。

## 1. 语言与构建
- C++ 标准：优先 C++20；如需降低标准，需在 [CMakeLists.txt](CMakeLists.txt) 同步调整并在本文档记录。
- 工具链：推荐 MSVC/Clang；启用较严格的告警并将告警视为错误。
- 跨平台：避免使用非必要的特定平台 API，必要时用特征宏隔离。

示例（CMake 告警与标准设置）:

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.21)
project(Engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4 /permissive- /EHsc /Zc:__cplusplus /WX)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Werror)
endif()
```

## 2. 目录与文件
- 源码放置于 [src/](src/)；第三方库置于 [external/](external/)；测试置于 [tests/](tests/)。
- 每个头文件只声明一个主要类型或主题；使用 `#pragma once`。
- 文件名使用小写短横线或下划线：如 `texture_loader.cpp`。

## 3. 格式化与静态检查
- 使用 clang-format 自动格式化（提交前执行）。
- 建议风格：基于 LLVM，120 列，4 空格缩进，函数参数对齐。
- 使用 clang-tidy 做静态检查，启用现代化与安全规则集。

示例（命令行）:

```bash
clang-format -i src/**/*.cpp src/**/*.h
clang-tidy src/**/*.cpp -- -Iexternal/SDL/include -Iexternal/glm
```

## 4. 命名约定
- 命名空间：小写短名，如 `gfx`、`core`。
- 类型名（类/结构体/枚举）：`PascalCase`，如 `TextureAtlas`。
- 变量与函数：`camelCase`；成员变量后缀 `_` 或前缀 `m_` 二选一（首选后缀 `_`）。
- 常量与枚举值：`SCREAMING_SNAKE_CASE`。
- 宏：`SCREAMING_SNAKE_CASE`，尽量少用。

## 5. 头文件包含顺序
1) 所属头（与 .cpp 匹配的 .h）
2) 同项目头（按路径字母序）
3) 第三方库（SDL、GLM 等）
4) 标准库

示例:

```cpp
// foo.cpp
#include "foo.h"              // 1. 本文件头
#include "core/log.h"         // 2. 项目头
#include <SDL3/SDL.h>         // 3. 第三方
#include <glm/glm.hpp>        // 3. 第三方
#include <vector>             // 4. 标准库
```

## 6. 命名空间与可见性
- 使用内联命名空间组织版本；避免在头文件使用 `using namespace`。
- 限定符最小化可见性：类型与函数默认放在匿名命名空间仅限本翻译单元使用（.cpp）。

## 7. 类与结构体
- 优先 `struct` 表示简单聚合，`class` 表示封装行为。
- 禁止裸 `new/delete`；使用 RAII 与智能指针。
- 明确默认、拷贝、移动语义；必要时 `=delete`。

示例:

```cpp
class Texture {
public:
  Texture() = default;
  explicit Texture(std::string path);
  Texture(const Texture&) = delete;
  Texture& operator=(const Texture&) = delete;
  Texture(Texture&&) noexcept = default;
  Texture& operator=(Texture&&) noexcept = default;
  ~Texture() = default;

  void bind() const;

private:
  std::string path_;
};
```

## 8. 函数与接口
- 单一职责；函数不超过 ~60 行；必要时拆分。
- 参数传递：大对象用 `const T&` 或 `T&&`；标量按值。
- 不在接口抛异常；跨边界用 `expected` 风格返回或状态码。

示例:

```cpp
[[nodiscard]] std::optional<Texture> loadTexture(const std::string& path) {
  // ...
  return std::nullopt;
}
```

## 9. 变量、常量与初始化
- 使用统一初始化与 `auto`（在不损害可读性时）。
- 常量使用 `constexpr` 或 `const`，并置于匿名命名空间或 `constexpr` 内联变量。
- 延迟初始化以避免无谓开销。

## 10. 资源管理与智能指针
- 拥有语义用 `std::unique_ptr`；共享明确需要才用 `std::shared_ptr`。
- 与 C API 交互用自定义删除器包装。

示例（SDL 资源包装）:

```cpp
using SDLSurfacePtr = std::unique_ptr<SDL_Surface, void(*)(SDL_Surface*)>;

inline SDLSurfacePtr makeSurface(SDL_Surface* raw) {
  return SDLSurfacePtr(raw, SDL_FreeSurface);
}
```

## 11. 错误处理与日志
- 可预期错误：返回 `std::expected<T, E>` 或 `std::optional<T>` 搭配日志。
- 不可恢复错误：`assert` 在调试，日志+失败快出在发布。
- 统一日志接口，支持级别和模块。

示例:

```cpp
enum class Error { FileNotFound, InvalidFormat };
using Result = std::expected<Texture, Error>;

Result loadTexture2(const std::string& path);
```

## 12. 并发与多线程
- 明确所有权与生命周期；避免共享可变状态。
- 使用 `std::jthread`、`std::mutex`、`std::atomic` 等标准原语。
- 在线程间传递数据优先消息队列/任务系统。

## 13. 性能与内存
- 优先算法级优化；避免过早优化。
- 避免不必要的拷贝；考虑 `string_view`、`span`。
- 使用 `-O2`/`-O3` 与 PGO/LLD 等工具链优化。

## 14. 注释与文档
- 注释解释“为什么”，而非“做了什么”。
- 公共 API 使用 Doxygen 风格注释；生成站点存于 [docs/](docs/)。

示例:

```cpp
/// 加载纹理
/// \param path 纹理路径（相对或绝对）
/// \return 成功时返回 Texture，否则返回 std::nullopt
[[nodiscard]] std::optional<Texture> loadTexture(const std::string& path);
```

## 15. 测试
- 使用 GoogleTest/CTest；测试与被测代码同名，后缀 `_test.cpp`。
- 覆盖核心逻辑与边界条件；CI 中运行全部测试。

示例（CMake 片段）:

```cmake
enable_testing()
add_executable(texture_loader_test tests/texture_loader_test.cpp)
target_link_libraries(texture_loader_test PRIVATE gtest_main project_lib)
add_test(NAME texture_loader_test COMMAND texture_loader_test)
```

## 16. CMake 规范
- 每个模块使用 `add_library(module_name ...)` 并设置最小可见性（`PRIVATE`/`PUBLIC`/`INTERFACE`）。
- 使用 `target_include_directories`、`target_compile_features`、`target_link_libraries` 等目标属性，而非全局变量。
- 路径使用相对工程根路径；避免硬编码绝对路径。

## 17. 第三方库（SDL3 / GLM）
- SDL：错误通过 `SDL_GetError()` 记录；资源用 RAII 包装。
- GLM：启用显式构造与精度限定；避免隐式转换。

示例（GLM 配置）:

```cpp
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
```

## 18. 代码审查
- 所有合并需经至少一人审查；审查关注可读性、正确性与边界。
- 变更尽量小而频繁；附带动机与影响说明。

## 19. 提交信息规范（Conventional Commits）
- 格式：`<type>(scope): <subject>`
- 常见 type：`feat`/`fix`/`refactor`/`docs`/`test`/`build`/`ci`/`perf`/`chore`
- 示例：`feat(renderer): add streaming texture support`

## 20. 分支策略
- `main` 稳定可发布；`dev` 日常集成；功能分支 `feature/*`。
- 通过 PR 合并；禁止直接推送 `main`。

## 21. 提交流程（建议）
1) 运行格式化与静态检查
2) 运行全部单测
3) 更新文档与变更记录
4) 提交并发起 PR

## 22. 例示代码片段（风格对比）

正确:

```cpp
[[nodiscard]] std::optional<int> parseInt(std::string_view s) {
  int value{};
  // TODO(core): 支持十六进制
  try {
    size_t idx{};
    value = std::stoi(std::string(s), &idx);
    if (idx != s.size()) return std::nullopt;
    return value;
  } catch (...) {
    return std::nullopt;
  }
}
```

错误:

```cpp
int parse_int(std::string s) {
  int v = 0;
  v = std::stoi(s);
  return v; // 未处理错误
}
```

---

附：推荐在仓库根添加 `.clang-format` 与 `.clang-tidy`，并在 CI 中强制检查。