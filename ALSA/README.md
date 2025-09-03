## ALSA 子系统
对我现在来说ALSA子系统说陌生也陌生说熟悉吧，目前手上一个音频类的项目后续的优化就需要采用ALSA子系统，当然这也是学习音视频开发绕不过的大山了，前面学习了理论这里呢着重于实践和代码能力，来解决ALSA编程过少的问题

###  ALSA_CAPTURE 类设计
在ALSA框架中，驱动层适配了硬件的驱动，让我们无需关心底层的驱动问题，我们需要通过`alsa-lib`提供的接口来操作硬件即可；即ALSA的采集操作进行封装类

在设计音频采集类的时候我们最容易想到什么？没错那就是音频的参数，设备名，等以及缓冲区大小和周期大小等重要参数，所以这些参数也是我们需要封装在类里面的

这里主包采用CMake来构建项目了，makefile也可以的，更多的是看自己的选择

音频采集类设计
```cpp
class AlsaCapture
    {
    public:
    public:
        // 构造函数 
        AlsaCapture(const std::string &device = "default",
                    int sample_rate = 44100,
                    int channels = 2);

        // 析构函数
        ~AlsaCapture();

        // 打开设备
        bool Open();

        // 关闭设备
        void Close();

        // 读取一帧音频数据
        bool ReadFrame(uint8_t *buffer, size_t buffer_size, int *frames_read);

        // 恢复设备（出错后）
        bool Recover();
        
        // 获取设备属性
        std::string GetDevice() const;
        int GetSampleRate() const ;
        int GetChannels() const ;
        int GetBytesPerSample() const; 
        snd_pcm_uframes_t GetBufferSize() const;
        snd_pcm_uframes_t GetPeriodSize() const;
        snd_pcm_format_t GetFormat() const;
        // 设备是否已打开
        bool IsOpened() const ;
        // 设置格式
        bool SetFormat(snd_pcm_format_t format);

    private:
        // 设备路径
        std::string m_device;
        // 音频参数
        // 音频采样率 通道 PCM句柄 缓冲区大小（周期大小为基数） 一个周期大小
        int                 m_sample_rates;
        int                 m_channels;
        snd_pcm_t *         m_pcm_handle;
        snd_pcm_uframes_t   m_buffer_size_;
        snd_pcm_uframes_t   m_period_size_;
        snd_pcm_format_t    m_formate;
    };
```

构建项目的CMake示例
```CMake
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