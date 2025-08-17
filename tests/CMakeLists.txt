# 测试框架配置（始终使用内置的 googletest，避免与系统已安装版本产生运行库/配置不一致）
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)

# 让 googletest 使用与父项目一致的运行时库（Debug 用 /MDd，Release 用 /MD），
# 避免与 Debug 目标链接时出现 RuntimeLibrary/_ITERATOR_DEBUG_LEVEL 不匹配。
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# 测试可执行文件
add_executable(luster_tests
    test_main.cpp
    test_vulkan_init.cpp
)

# 链接测试框架
target_link_libraries(luster_tests
    PRIVATE
    GTest::gtest_main
    luster::core
    luster::thirdparty
)

# 设置测试属性
set_target_properties(luster_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tests"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)

# 注册测试
include(GoogleTest)
gtest_discover_tests(luster_tests
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)

# 创建测试预设
# Create a convenience target to run tests. Ensure multi-config generators pass the config name.
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} -C $<CONFIG> --output-on-failure
    DEPENDS luster_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)