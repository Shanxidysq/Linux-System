#ifndef __ALSA_CAPTURE_HPP__
#define __ALSA_CAPTURE_HPP__
// 防止头文件重复包含
#include <iostream>
#include <string>
#include <cstdint>
#include <alsa/asoundlib.h>
#include <alsa/pcm.h>
#include "wav.hpp"
// 个人命名空间
namespace ox
{
    // 音频采集类封装
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
        
        // 增加方法适配aplay 适配报头信息
        void SetWavFrames(WAVHeader& wav) const;
    private:
        // 设备路径
        std::string m_device;
        // 音频参数
        // 音频采样率 通道 PCM句柄 缓冲区大小（周期大小为基数） 一个周期大小
        int                 m_sample_rate;
        int                 m_channels;
        snd_pcm_t *         m_pcm_handle;
        snd_pcm_uframes_t   m_buffer_size;
        snd_pcm_uframes_t   m_period_size;
        snd_pcm_format_t    m_format;
    };
}
#endif