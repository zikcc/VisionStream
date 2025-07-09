cmake_minimum_required(VERSION 3.14)
project(VisionTests LANGUAGES CXX)
# 自动发现并注册测试
include(GoogleTest)
set(CMAKE_CXX_STANDARD 17)

find_package(GTest REQUIRED)
# 测试可执行文件
add_executable(v4l2_tests
    test_v4l2.cpp
)
# 测试可执行文件
add_executable(ar_tests
    test_ar.cpp
)
# 链接依赖库（包括 vision、gtest、线程库）
foreach(test_target IN ITEMS v4l2_tests ar_tests)
    target_link_libraries(${test_target}
        PRIVATE
            vision
            GTest::GTest
            GTest::Main
    )
    gtest_discover_tests(${test_target})
endforeach()
