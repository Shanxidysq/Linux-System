cmake_minimum_required(VERSION 3.10)

# 项目设置
project(SampleProject C)
set(TARGET main)
# 设置版本号
set(VERSION_MAJOR 1)
set(VERSION_MINOR 0)
set(VERSION_PATCH 0)

# 添加版本定义
add_definitions(-DVERSION_MAJOR=${VERSION_MAJOR})
add_definitions(-DVERSION_MINOR=${VERSION_MINOR})
add_definitions(-DVERSION_PATCH=${VERSION_PATCH})

# 设置输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

# 包含头文件目录
include_directories(${CMAKE_SOURCE_DIR}/inc)

# 添加可执行文件
aux_source_directory(src SRC_LIST)
add_executable(${TARGET} ${SRC_LIST} main.c)

# 设置目标属性
set_target_properties(${TARGET} PROPERTIES
    OUTPUT_NAME "${TARGET}"
    VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}
)
