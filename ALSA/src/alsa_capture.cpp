#include "alsa_capture.hpp"

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
        // Close();
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
            std::cerr << "malloc hw params error: " << snd_strerror(ret)<< std::endl;
            return false;
        }
        // 获取设备硬件参数
        ret = snd_pcm_hw_params_any(m_pcm_handle, params);
        if (ret < 0)
        {
            std::cerr << "get device params error: " <<snd_strerror(ret) << std::endl;
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
        return true;
    }
}