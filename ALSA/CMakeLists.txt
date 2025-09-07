# 指定CMake的最低版本
cmake_minimum_required(VERSION 3.0)

# 项目名称
project(ALSA)

# exe name 
set(TARGET play)

# 添加链接库
# 在arm-linux-gcc 交叉编译环境下是否也可以呢
find_package(ALSA REQUIRED)

# 增加inc路径
include_directories(
    ${PROJECT_SOURCE_DIR}/inc
    ${ALSA_INCLUDE_DIRS}  # 添加ALSA头文件路径
)

# 收集src目录下的源文件到SOURCE_LIST 
aux_source_directory(src SOURCE_LIST)

# test capture
# add_executable(${TARGET} ${SOURCE_LIST} main.cpp)

# test playback
add_executable(${TARGET} ${SOURCE_LIST} testplayback.cpp)

# 链接库 - 放在add_executable之后
target_link_libraries(${TARGET} PRIVATE ${ALSA_LIBRARIES})

# 设置exe文件的生成路径
# set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)