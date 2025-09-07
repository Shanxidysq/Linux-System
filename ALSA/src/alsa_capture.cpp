#include "alsa_capture.hpp"
#include "wav.hpp"
namespace ox
{
    // 构造函数
    // 这里的设备名称 通常就是 hw:x,y格式 x第x个声卡里面的设备y
    AlsaCapture::AlsaCapture(const std::string &device, int sample_rate, int channels)
        : m_device(device),               // 设备名称（如 "hw:0", "default"）
          m_sample_rate(sample_rate),     // 采样率（如 44100Hz）
          m_channels(channels),           // 通道数（1=单声道，2=立体声）
          m_pcm_handle(nullptr),          // ALSA设备句柄
          m_buffer_size(0),               // 缓冲区大小（帧数）
          m_period_size(0),               // 周期大小（帧数）
          m_format(SND_PCM_FORMAT_S16_LE) // 默认使用16位有符号小端格式
    {
        std::cout << "初始化音频采集设备: " << device << std::endl;
        std::cout << "采样率: " << sample_rate << "Hz" << std::endl;
        std::cout << "通道数: " << channels << std::endl;
    }
    // 析构函数
    AlsaCapture::~AlsaCapture()
    {
        Close();
#if 0
        if(nullptr != m_pcm_handle)
        {
            // 持有pcm设备资源需要释放
            free(m_pcm_handle);
            m_pcm_handle = nullptr;
        }
#endif
    }

    // 打开PCM设备
    bool AlsaCapture::Open()
    {
        // 函数失败返回值<0
        int ret = snd_pcm_open(&m_pcm_handle, m_device.c_str(), SND_PCM_STREAM_CAPTURE, 0);
        if (ret < 0)
        {
            std::cerr << "open pcm device error" << std::endl;
            return false;
        }
        // 分配硬件参数
        // 后续可以通过param硬件参数来控制pcm设备
        // 在下面的设置接口中传入params，均是让params保存当前最新的硬件参数数值
        snd_pcm_hw_params_t *params;
        // 分配空间
        snd_pcm_hw_params_malloc(&params);
        if (nullptr == params)
        {
            std::cerr << "malloc hw params error: " << snd_strerror(ret) << std::endl;
            return false;
        }
        // 获取设备硬件参数
        ret = snd_pcm_hw_params_any(m_pcm_handle, params);
        if (ret < 0)
        {
            std::cerr << "get device params error: " << snd_strerror(ret) << std::endl;
            return false;
        }

        // 设置访问类型为交错模式（左右声道数据交错存储）
        ret = snd_pcm_hw_params_set_access(m_pcm_handle, params,
                                           SND_PCM_ACCESS_RW_INTERLEAVED);
        if (ret < 0)
        {
            std::cerr << "无法设置访问类型: " << snd_strerror(ret) << std::endl;
            return false;
        }

        // 设置采样格式为16位有符号小端
        ret = snd_pcm_hw_params_set_format(m_pcm_handle, params, SND_PCM_FORMAT_S16_LE);
        if (ret < 0)
        {
            std::cerr << "无法设置采样格式: " << snd_strerror(ret) << std::endl;
            return false;
        }

        // 设置通道数
        ret = snd_pcm_hw_params_set_channels(m_pcm_handle, params, m_channels);
        if (ret < 0)
        {
            std::cerr << "无法设置通道数: " << snd_strerror(ret) << std::endl;
            return false;
        }

        // 设置采样率
        unsigned int rate = m_sample_rate;
        ret = snd_pcm_hw_params_set_rate_near(m_pcm_handle, params, &rate, 0);
        if (ret < 0)
        {
            std::cerr << "无法设置采样率: " << snd_strerror(ret) << std::endl;
            return false;
        }

        // 计算并设置缓冲区大小（100ms的缓冲）
        snd_pcm_uframes_t buffer_size = rate / 10; // 100ms = 0.1秒
        ret = snd_pcm_hw_params_set_buffer_size_near(m_pcm_handle, params,
                                                     &buffer_size);
        if (ret < 0)
        {
            std::cerr << "无法设置缓冲区大小: " << snd_strerror(ret) << std::endl;
            return false;
        }
        m_buffer_size = buffer_size;

        // 计算并设置周期大小（缓冲区大小的1/4）
        snd_pcm_uframes_t period_size = buffer_size / 4;
        ret = snd_pcm_hw_params_set_period_size_near(m_pcm_handle, params,
                                                     &period_size, 0);
        if (ret < 0)
        {
            std::cerr << "无法设置周期大小: " << snd_strerror(ret) << std::endl;
            return false;
        }
        m_period_size = period_size;

        // 应用硬件参数
        ret = snd_pcm_hw_params(m_pcm_handle, params);
        if (ret < 0)
        {
            std::cerr << "无法应用硬件参数: " << snd_strerror(ret) << std::endl;
            return false;
        }

        // 准备设备开始采集
        ret = snd_pcm_prepare(m_pcm_handle);
        if (ret < 0)
        {
            std::cerr << "无法准备设备: " << snd_strerror(ret) << std::endl;
            return false;
        }

        std::cout << "音频设备已打开" << std::endl;
        std::cout << "缓冲区大小: " << m_buffer_size << " 帧" << std::endl;
        std::cout << "周期大小: " << m_period_size << " 帧" << std::endl;
        std::cout << "位深度大小：" << GetBytesPerSample() << "字节数" << std::endl;
        return true;
    }
    // 关闭设备资源
    void AlsaCapture::Close()
    {
        // 资源不为空释放资源
        if (nullptr != m_pcm_handle)
        {
            // 释放资源
            snd_pcm_close(m_pcm_handle);
            m_pcm_handle = nullptr;
            std::cout << " pcm设备已经关闭 " << std::endl;
        }
    }

    // 从设备寄存器中读取数据
    bool AlsaCapture::ReadFrame(uint8_t *buffer, size_t buffer_size, int *frames_read)
    {
        // 未持有pcm设备资源
        if (nullptr == m_pcm_handle)
        {
            std::cerr << "设备未打开" << std::endl;
            return false;
        }

        // 计算缓冲区帧数
        // 缓冲区帧数 = 字节数/通道数/位深度
        // GetBytesPerSample() 每个采样字节数
        // 计算缓冲区可以装的帧数
        int frames = buffer_size / m_channels / GetBytesPerSample();
        // 如果帧数大于一个周期大小，规定为一个周期
        if (frames > m_period_size)
        {
            frames = m_period_size;
        }

        // 读取音频数据 读取一帧数据
        int err = snd_pcm_readi(m_pcm_handle, buffer, frames);
        if (err < 0)
        {
            if (err == -EPIPE)
            {
                // err 缓冲区溢出 重新准备设备
                err = snd_pcm_prepare(m_pcm_handle);
                if (err < 0)
                {
                    std::cerr << "恢复设备失败" << std::endl;
                    return false;
                }
            }
            else
            {
                //
                std::cerr << "设备错误" << std::endl;
                return false;
            }
        }
        *frames_read = err;
        std::cout << "采集数据成功" << std::endl;
        return true;
    }

    // 获取每个采样的字节数
    // 根据每个采样的位深度等信息返回采样字节数
    int AlsaCapture::GetBytesPerSample() const
    {
        switch (m_format)
        {
        case SND_PCM_FORMAT_S8:
            return 1; // 8位有符号
        case SND_PCM_FORMAT_U8:
            return 1; // 8位无符号
        case SND_PCM_FORMAT_S16_LE:
            return 2; // 16位有符号小端
        case SND_PCM_FORMAT_S16_BE:
            return 2; // 16位有符号大端
        case SND_PCM_FORMAT_U16_LE:
            return 2; // 16位无符号小端
        case SND_PCM_FORMAT_U16_BE:
            return 2; // 16位无符号大端
        case SND_PCM_FORMAT_S24_LE:
            return 3; // 24位有符号小端
        case SND_PCM_FORMAT_S24_BE:
            return 3; // 24位有符号大端
        case SND_PCM_FORMAT_U24_LE:
            return 3; // 24位无符号小端
        case SND_PCM_FORMAT_U24_BE:
            return 3; // 24位无符号大端
        case SND_PCM_FORMAT_S32_LE:
            return 4; // 32位有符号小端
        case SND_PCM_FORMAT_S32_BE:
            return 4; // 32位有符号大端
        case SND_PCM_FORMAT_U32_LE:
            return 4; // 32位无符号小端
        case SND_PCM_FORMAT_U32_BE:
            return 4; // 32位无符号大端
        case SND_PCM_FORMAT_FLOAT_LE:
            return 4; // 32位浮点小端
        case SND_PCM_FORMAT_FLOAT_BE:
            return 4; // 32位浮点大端
        case SND_PCM_FORMAT_FLOAT64_LE:
            return 8; // 64位浮点小端
        case SND_PCM_FORMAT_FLOAT64_BE:
            return 8; // 64位浮点大端
        default:
            std::cerr << "不支持的音频格式" << std::endl;
            return 2; // 默认返回2字节
        }
    }

    // 获取缓冲区大小
    snd_pcm_uframes_t AlsaCapture::GetBufferSize() const
    {
        return m_buffer_size;
    }
    // 获取一个周期大小
    snd_pcm_uframes_t AlsaCapture::GetPeriodSize() const
    {
        return m_period_size;
    }
    // 设备采样模式
    bool AlsaCapture::SetFormat(snd_pcm_format_t format)
    {
        if (nullptr != m_pcm_handle)
        {
            std::cerr << "设备已经打开无法设备format格式" << std::endl;
            return false;
        }
        this->m_format = format;
        return true;
    }

    // 恢复设备
    bool AlsaCapture::Recover()
    {
        // 设备没有打开无法恢复
        if (nullptr == m_pcm_handle)
        {
            std::cerr << " 设备未打开，无法恢复 " << std::endl;
            return false;
        }
        // 调用alsa-lib恢复设备
        int err = snd_pcm_prepare(m_pcm_handle);
        if (err < 0)
        {
            // 设备恢复失败
            std::cerr << "设备恢复关系" << std::endl;
            return false;
        }

        // 重新启动设备采集
        err = snd_pcm_start(m_pcm_handle);
        if (err < 0)
        {
            // 设备启动采集失败
            std::cerr << "设备启动采集失败" << std::endl;
            return false;
        }
        std::cout << " 设备Recover正常 " << std::endl;
        return true;
    }

    // 获取设备属性
    std::string AlsaCapture::GetDevice() const
    {
        return m_device;
    }
    int AlsaCapture::GetSampleRate() const
    {
        return m_sample_rate;
    }
    int AlsaCapture::GetChannels() const
    {
        return m_channels;
    }

    // 设备是否已打开
    bool AlsaCapture::IsOpened() const
    {
        return m_pcm_handle != nullptr;
    }

    void AlsaCapture::SetWavFrames(WAVHeader &wav) const
    {
        wav.channels = m_channels;
        wav.sample_rate = m_sample_rate;
        wav.bits_per_sample = 16;              // 固定
        wav.block_align = m_channels * 16 / 8; // 即 channels*2
        wav.byte_rate = m_sample_rate * wav.block_align;
    }
}