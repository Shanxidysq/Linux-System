# CMake 交叉编译构建项目
在现代的c++项目中，使用makefile构建项目已经是很麻烦的了或者是很难学习考虑到全面的makefile语法

所以采用CMake管理大型项目和交叉编译器的管理

## 指定交叉编译文件
使用CMake进行交叉编译构建第一步需要我们创建工具链文件，例如我们使用的x86_64架构目标平台imx6ull是ARM架构所我们需要创建一个工具链文件

例如我们的ARM-A7内核，是32，所以需要创建文件`toolchain-arm-linux-gnueabihf.cmake`

`arm-linux-gnueabihf-gcc`
`arm-linux-gnueabihf-g++`
我门需要使用到的两个交叉编译工具，用来编译我们的`C/C++`程序，编译到目标平台的话我们创建一个工具链文件，在需要进行跨平台编译验证的时候打开文件，不需要跨平台验证的时候即关闭文件即可

toolchain-arm-linux-gnueabihf.cmake文件内容如下
```shell
# ARM Linux交叉编译工具链配置
# 适用于ARMv7架构，硬浮点ABI

# 1.设置系统信息 指定目标平台的系统和体系结构
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# 2.指定工具链前缀和路径 指定当前交叉编译工具链的路径
set(TOOLCHAIN_PREFIX arm-linux-gnueabihf)
set(TOOLCHAIN_PATH /opt/toolchains/gcc-linaro-7.5.0-2019.12-x86_64_arm-linux-gnueabihf)

# 3.设置编译器 工具交叉编译工具路径设置交叉编译器 gcc/g++
set(CMAKE_C_COMPILER ${TOOLCHAIN_PATH}/bin/${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PATH}/bin/${TOOLCHAIN_PREFIX}-g++)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PATH}/bin/${TOOLCHAIN_PREFIX}-gcc)

# 4.设置工具选项和配置
# 设置归档工具(Archiver)，用于创建和管理静态库(.a文件)
set(CMAKE_AR ${TOOLCHAIN_PATH}/bin/${TOOLCHAIN_PREFIX}-ar CACHE FILEPATH "Archiver")

# 设置随机化归档索引工具，用于为静态库生成符号索引
set(CMAKE_RANLIB ${TOOLCHAIN_PATH}/bin/${TOOLCHAIN_PREFIX}-ranlib CACHE FILEPATH "Ranlib")

# 设置剥离工具，用于从可执行文件中删除调试符号，减小文件大小
set(CMAKE_STRIP ${TOOLCHAIN_PATH}/bin/${TOOLCHAIN_PREFIX}-strip CACHE FILEPATH "Strip")

# 设置目标文件复制工具，用于转换和复制目标文件
set(CMAKE_OBJCOPY ${TOOLCHAIN_PATH}/bin/${TOOLCHAIN_PREFIX}-objcopy CACHE FILEPATH "Objcopy")

# 设置目标文件转储工具，用于显示目标文件信息
set(CMAKE_OBJDUMP ${TOOLCHAIN_PATH}/bin/${TOOLCHAIN_PREFIX}-objdump CACHE FILEPATH "Objdump")

# 设置符号列表工具，用于列出目标文件中的符号
set(CMAKE_NM ${TOOLCHAIN_PATH}/bin/${TOOLCHAIN_PREFIX}-nm CACHE FILEPATH "Nm")

# 设置ELF文件分析工具，用于显示ELF格式文件的信息
set(CMAKE_READELF ${TOOLCHAIN_PATH}/bin/${TOOLCHAIN_PREFIX}-readelf CACHE FILEPATH "Readelf")


# 5.设置目标系统根目录（sysroot） 也就是编译工具链的目录告诉编译器在哪里可以找到目标系统的一些文件和库
set(CMAKE_SYSROOT ${TOOLCHAIN_PATH}/${TOOLCHAIN_PREFIX}/libc)
set(CMAKE_FIND_ROOT_PATH ${TOOLCHAIN_PATH}/${TOOLCHAIN_PREFIX}/libc)

# 6.设置编译器标志  这个就是配置编译器选项 例如编译多线程 -std=c++11 -lpthread 
# 指定c++11标准，并且连接线程库
set(CMAKE_C_FLAGS "-march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard" CACHE STRING "C Compiler Flags")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" CACHE STRING "C++ Compiler Flags")

# 设置查找策略
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# 设置pkg-config环境变量
set(ENV{PKG_CONFIG_SYSROOT_DIR} ${CMAKE_SYSROOT})
set(ENV{PKG_CONFIG_PATH} "${CMAKE_SYSROOT}/usr/lib/pkgconfig:${CMAKE_SYSROOT}/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_LIBDIR} "${CMAKE_SYSROOT}/usr/lib/pkgconfig:${CMAKE_SYSROOT}/usr/share/pkgconfig")

# 编译器测试设置（跳过编译器测试可以加快配置速度）
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)

# 输出配置信息
message(STATUS "Cross-compiling for ${CMAKE_SYSTEM_NAME} (${CMAKE_SYSTEM_PROCESSOR})")
message(STATUS "Using toolchain: ${TOOLCHAIN_PATH}")
message(STATUS "Using C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "Using C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "Using sysroot: ${CMAKE_SYSROOT}")
```