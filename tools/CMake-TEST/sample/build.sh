#!/bin/bash

# 项目根目录
PROJECT_ROOT=$(cd `dirname $0`; pwd)

# 构建目录
BUILD_DIR=${PROJECT_ROOT}/build

# 清理并创建构建目录
rm -rf ${BUILD_DIR}
mkdir -p ${BUILD_DIR}
cd ${BUILD_DIR}

# 运行CMake，指定工具链文件
cmake -DCMAKE_TOOLCHAIN_FILE=${PROJECT_ROOT}/toolchain/arm-linux-gnueabihf.cmake ..

# 编译
make
