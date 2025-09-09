# Cmake搭建Arm交叉编译环境

在ALSA库编程的时候，会遇到环境体系结构等不对称的问题，需要我们进行交叉编译
这里使用CMake搭建交叉编译环境

这是一个deepseek提供的CMake搭建交叉编译环境示例
CMake搭建交叉编译环境要点
1.指定目标的体系结构和系统
2.指定交叉编译路径
3.设置编译器选项等等
```CMake
# 设置目标系统类型
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# 指定交叉编译器的路径（根据你的实际安装路径调整）
set(TOOLCHAIN_DIR /usr) # 如果编译器在/usr/bin下，则设置为/usr
# 或者如果编译器在其他路径，例如/usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf/bin，则设置TOOLCHAIN_DIR为该路径的上一级

# 设置交叉编译器
set(CMAKE_C_COMPILER ${TOOLCHAIN_DIR}/bin/arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_DIR}/bin/arm-linux-gnueabihf-g++)

# 设置编译标志（根据需要添加）
set(CMAKE_CXX_FLAGS "-std=c++11" CACHE STRING "C++ flags" FORCE)

# 设置查找库的路径
set(CMAKE_FIND_ROOT_PATH ${TOOLCHAIN_DIR}/arm-linux-gnueabihf)

# 搜索模式：只在指定路径下查找库和头文件
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
```