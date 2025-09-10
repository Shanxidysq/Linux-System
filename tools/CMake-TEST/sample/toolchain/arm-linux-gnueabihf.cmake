# 1.设置目标的系统和架构
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# 2.设置编译器
set(CMAKE_C_COMPILER /usr/bin/arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER /usr/bin/arm-linux-gnueabihf-g++)

# 3.设置其他工具
set(CMAKE_AR /usr/bin/arm-linux-gnueabihf-ar)
set(CMAKE_NM /usr/bin/arm-linux-gnueabihf-nm)
set(CMAKE_OBJCOPY /usr/bin/arm-linux-gnueabihf-objcopy)
set(CMAKE_OBJDUMP /usr/bin/arm-linux-gnueabihf-objdump)
set(CMAKE_RANLIB /usr/bin/arm-linux-gnueabihf-ranlib)
set(CMAKE_STRIP /usr/bin/arm-linux-gnueabihf-strip)

# 4.设置目标系统的路径查找需要的库文件
set(CMAKE_SYSROOT /usr/bin/arm-linux-gnueabihf)
set(CMAKE_FIND_ROOT_PATH /usr/bin/arm-linux-gnueabihf)

# 5.设置查找策略
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# 6.设置编译器标志
set(CMAKE_C_FLAGS "-march=armv7-a -mfpu=neon -mfloat-abi=hard" CACHE STRING "C compiler flags")
set(CMAKE_CXX_FLAGS "-march=armv7-a -mfpu=neon -mfloat-abi=hard" CACHE STRING "C++ compiler flags")