# 使用CMake交叉编译ALSA

自己在实际开发过程中需要处理`alsa-lib`库的交叉编译问题，所以在这里详细说明一下交叉编译环境的搭建

## 1.交叉编译工具链文件

通过设置交叉编译工具链文件，让整个系统知道我们的工具链配置环境和位置`sysroot`即交叉编译工具链的根目录
```cmake
# 设置交叉编译
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# 设置工具链路径
set(TOOLCHAIN_DIR /usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf)

# 设置编译器
set(CMAKE_C_COMPILER ${TOOLCHAIN_DIR}/bin/arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_DIR}/bin/arm-linux-gnueabihf-g++)

# 设置其他工具
set(CMAKE_AR ${TOOLCHAIN_DIR}/bin/arm-linux-gnueabihf-ar)
set(CMAKE_NM ${TOOLCHAIN_DIR}/bin/arm-linux-gnueabihf-nm)
set(CMAKE_OBJCOPY ${TOOLCHAIN_DIR}/bin/arm-linux-gnueabihf-objcopy)
set(CMAKE_OBJDUMP ${TOOLCHAIN_DIR}/bin/arm-linux-gnueabihf-objdump)
set(CMAKE_RANLIB ${TOOLCHAIN_DIR}/bin/arm-linux-gnueabihf-ranlib)
set(CMAKE_STRIP ${TOOLCHAIN_DIR}/bin/arm-linux-gnueabihf-strip)

# 设置目标系统的根目录
# 注意：这里可能需要根据您的工具链实际结构进行调整
# SYSROOT 是工具链目录下的libc不是简单的工具链路径这里要分清楚
set(CMAKE_SYSROOT ${TOOLCHAIN_DIR}/arm-linux-gnueabihf/libc)
set(CMAKE_FIND_ROOT_PATH ${TOOLCHAIN_DIR}/arm-linux-gnueabihf/libc)

# 设置查找策略
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# 设置编译器标志
set(CMAKE_C_FLAGS "--sysroot=${TOOLCHAIN_DIR}/arm-linux-gnueabihf/libc -march=armv7-a -mfpu=neon -mfloat-abi=hard" CACHE STRING "C compiler flags")
set(CMAKE_CXX_FLAGS "--sysroot=${TOOLCHAIN_DIR}/arm-linux-gnueabihf/libc -march=armv7-a -mfpu=neon -mfloat-abi=hard" CACHE STRING "C++ compiler flags")
```

## 2.CMakeLists.txt文件

CMakeLists.txt文件设置了主要的编译规则，例如我们需要导入的库文件，等等
```cmake
cmake_minimum_required(VERSION 3.10)

# 项目设置 指定语言cpp
project(SampleProject)
set(TARGET player)

# 设置输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

# 包含头文件目录
include_directories(${CMAKE_SOURCE_DIR}/inc)

# 设置C++11标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加可执行文件
aux_source_directory(src SRC_LIST)

add_executable(${TARGET} ${SRC_LIST} main.cpp)

# 链接pthread库
target_link_libraries(${TARGET} pthread)

# 设置可执行文件输出路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
```


## 3.编译脚本 build.sh

如果配置了交叉编译工具链文件，要显示使用相应的规则，必须显示使用工具链文件

设置工具链文件路径
`TOOLCHAIN_FILE=${PROJECT_ROOT}/toolchain/arm-linux-gnueabihf.cmake`

运行CMake 确保使用的CMake工具链文件中的规则编译
`cmake -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE} ..`

```shell
#!/bin/bash

# 设置错误时退出
set -e

# 项目根目录
PROJECT_ROOT=$(cd `dirname $0`; pwd)
echo "项目根目录: $PROJECT_ROOT"

# 构建目录
BUILD_DIR=${PROJECT_ROOT}/build
echo "构建目录: $BUILD_DIR"

# 工具链文件路径
TOOLCHAIN_FILE=${PROJECT_ROOT}/toolchain/arm-linux-gnueabihf.cmake
echo "工具链文件: $TOOLCHAIN_FILE"

# 检查工具链文件是否存在
if [ ! -f "$TOOLCHAIN_FILE" ]; then
    echo "错误: 工具链文件不存在: $TOOLCHAIN_FILE"
    echo "请确保工具链文件路径正确"
    exit 1
fi

# 清理并创建构建目录
echo "清理构建目录..."
rm -rf ${BUILD_DIR}
mkdir -p ${BUILD_DIR}
cd ${BUILD_DIR}

# 运行CMake，指定工具链文件
echo "运行CMake..."
cmake -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE} ..

# 检查CMake是否成功
if [ $? -ne 0 ]; then
    echo "CMake配置失败"
    exit 1
fi

# 编译
echo "开始编译..."
make -j$(nproc)  # 使用所有可用的CPU核心加速编译

# 检查编译是否成功
if [ $? -ne 0 ]; then
    echo "编译失败"
    exit 1
fi

echo "编译完成！"
echo "可执行文件位于: ${PROJECT_ROOT}/bin/"
```


## 4.alsa

alsa开发引入相关库文件之后，会报错，因为在sysroot交叉编译器的根文件环境下是没有相关的库文件的，所以需要我们对交叉编译器的相关环境进行配置

```c
#include <iostream>
#include <thread>
#include <alsa/asoundlib.h>
...

```

我们通常自己使用的是编译出来x86架构的alsa代码，而我们需要编译出来arm架构的代码就需要我们自己将alsa的源码拉下来编译，然后配置交叉编译器查找编译出来的arm架构的代码

1.获取源码
```shell
wget http://www.alsa-project.org/files/pub/lib/alsa-lib-1.2.7.1.tar.bz2
tar -xvf alsa-lib-1.2.7.1.tar.bz2
cd alsa-lib-1.2.7.1
```

2.解压
```shell
sudo tar -vxf ...
```

3.配置编译选项确保编译出来是arm架构的代码
```shell
./configure \
  --host=arm-linux-gnueabihf \
  --prefix=$(pwd)/_install \
  --with-configdir=/usr/share/arm-alsa \
  CC=/usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc \
  CXX=/usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-g++ \
  AR=/usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-ar \
  RANLIB=/usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-ranlib \
  --enable-shared \
  --disable-python
```shell

4.验证编译出来是否为arm架构的库文件

```shell
ox@ox-virtual-machine:/usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf/alsa-lib-1.2.7.1/_install/lib$ file libasound.so.2.0.0 
libasound.so.2.0.0: ELF 32-bit LSB shared object, ARM, EABI5 version 1 (SYSV), dynamically linked, BuildID[sha1]=b7d3d7d62385c691c46da185784c01d5a3397e70, with debug_info, not stripped
```
file一下库文件即可知道是否属于arm架构哟，因为我在之前配置踩了很多坑


5.就是在CMakeLists.txt中添加对应规则
下面制定了编译出来的alsa目录
设置inc规则和lib连接规则
```c
set(TOOLCHAIN_DIR /usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf)
# 项目设置 指定语言cpp
project(SampleProject)
set(TARGET player)

# 制定编译出来的arm架构文件路径
set(ALSA_ROOT ${TOOLCHAIN_DIR}/alsa-lib-1.2.7.1/_install)
set(ALSA_INCLUDE_DIR ${ALSA_ROOT}/include)
set(ALSA_LIBRARY   ${ALSA_ROOT}/lib/libasound.so)
```