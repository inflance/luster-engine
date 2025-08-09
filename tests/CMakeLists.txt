# 测试框架配置
find_package(GTest QUIET)

if(NOT GTest_FOUND)
    # 使用 FetchContent 获取 GoogleTest
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.12.1
    )
    
    # 对于 Windows: 防止覆盖父项目的编译器/链接器设置
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    
    FetchContent_MakeAvailable(googletest)
endif()

# 测试可执行文件
add_executable(luster_tests
    test_main.cpp
    test_vulkan_init.cpp
)

# 链接测试框架
target_link_libraries(luster_tests 
    PRIVATE 
    GTest::gtest_main
    GTest::gmock_main
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
if(NOT CMAKE_PRESET_FILE)
    add_custom_target(run_tests
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        DEPENDS luster_tests
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endif()