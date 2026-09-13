cmake_minimum_required(VERSION 3.31)
project(MyProject)

# 默认 Release：未显式指定 CMAKE_BUILD_TYPE 时防止落入无优化的 -O0
# （-O0 会慢约 20 倍且不报错；显式传 -DCMAKE_BUILD_TYPE=Debug 等仍优先）
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR}/include)

# 自动收集 src 文件夹下所有 .cpp 文件（不用一个个加！）
# CONFIGURE_DEPENDS：新增/删除源文件后自动重新运行本脚本，
# 否则 GLOB 结果停留在首次配置，新文件不会被编译，链接期才报符号未定义。
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
        ${CMAKE_SOURCE_DIR}/src/*.cpp
)

# 最终可执行文件：main.cpp + 所有src下的cpp
add_executable(${PROJECT_NAME}
        main.cpp
        ${SRC_FILES}
)


set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)
find_package(spdlog REQUIRED)

target_link_libraries(MyProject PRIVATE spdlog::spdlog)


