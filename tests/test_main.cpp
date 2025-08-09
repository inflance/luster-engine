#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

// GoogleTest 主函数入口
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    // 配置日志级别
    spdlog::set_level(spdlog::level::warn);

    return RUN_ALL_TESTS();
}