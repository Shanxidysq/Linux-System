

**为什么要使用CMake？**

因为再Linux开发中，我们通常需要集成很多第三方库或者是交叉编译等等，编译环境不再像使用vs2022这样的ide一样方便了，需要我们自己指定头文件路径，源文件路径以及编译选项等，如果是一两个源码文件可以简单使用gcc进行执行 -l指定链接库进行编译，但是这样子呢在大型项目当中会降低我们的开发效率，并且编译一次



```cmake
# 指定CMake的最低版本
cmake_minimum_required(VERSION 3.0)

# 项目名称
project(ALSA)

# exe name - 修正拼写错误
set(TARGET main)

# 添加链接库
find_package(ALSA REQUIRED)

# 增加inc路径
include_directories(
    ${PROJECT_SOURCE_DIR}/inc
    ${ALSA_INCLUDE_DIRS}  # 添加ALSA头文件路径
)

# 收集src目录下的源文件到SOURCE_LIST 
aux_source_directory(src SOURCE_LIST)

# 生成exe
add_executable(${TARGET} ${SOURCE_LIST} main.cpp)

# 链接库 - 放在add_executable之后
target_link_libraries(${TARGET} PRIVATE ${ALSA_LIBRARIES})

# 设置exe文件的生成路径
# set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
```

