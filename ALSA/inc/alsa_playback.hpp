#ifndef __ALSA_PLAYBACK_H__
#define __ALSA_PLAYBACK_H__
#include <iostream>
#include <string>
#include <alsa/asoundlib.h>

// 设计播放类
namespace ox
{
    // AlsaCapture  采集类读缓冲区
    // AlsaPlayback 播放类写缓冲区
    class AlsaPlayback
    {
    public:
        AlsaPlayback(const std::string device, int sample_rate, int channels);
        ~AlsaPlayback();
        // 打开设备
        bool Open();
        // 关闭设备
        void Close();
        // 缓冲区中写数据
        bool WriteFrame(const uint8_t *buffer, size_t buffer_size, int *frames_written);
        // 恢复设备
        bool Recover(int err);
        // 获取一个采样字节数
        int GetBytesPerSample() const;
        // 获取采样格式
        snd_pcm_format_t GetFormat() const;
        // 设置采样格式
        bool SetFormat(snd_pcm_format_t format);

    private:
        // 设置参数
        bool SetParams();
        std::string m_device;
        int m_sample_rate;
        int m_channels;
        snd_pcm_t *m_pcm_handle;       // 修改为正确的类型
        snd_pcm_format_t m_format; // 添加格式成员变量
    };
}
#endif