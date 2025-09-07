#include <iostream>
#include <fstream>
#include <csignal>
#include <atomic>
#include "alsa_capture.hpp"
#include "wav.hpp"
using namespace ox;
using namespace std;
// 全局运行标致
atomic<bool> runing;
int pcmBytes = 0;
std::ofstream w_file("/home/ox/work/Linux-System/ALSA/audio/raw.wav", std::ios::binary);

ox::WAVHeader header;
// 注册ctrl+c中断处理函数封装完整帧头进去

void Handler(int)
{
    runing = false;
}
int main()
{
    // 打开设备
    ox::AlsaCapture cap("hw:0,0", 44100);
    // 绑定中断处理函数
    std::signal(SIGINT, Handler);
    cap.Open();
    cap.SetWavFrames(header);

    // 读取一帧的数据
    // 采集音频数据
    // 二进制的方式写入数据

    if (!w_file.is_open())
    {
        cout << "文件打开失败" << endl;
        exit(1);
    }

    // 写入帧头数据
    printf("RIFF %.4s  ch=%u  rate=%u  bits=%u  data_size=%u\n",
           header.riff, header.channels, header.sample_rate,
           header.bits_per_sample, header.data_size);
    w_file.write((const char *)&header, sizeof(WAVHeader));
    while (runing)
    {
        uint8_t buffer[1024] = {0};
        int len = 0;
        // 录音
        cap.ReadFrame(buffer, 1024, &len);
        pcmBytes += len;
        cout << "采集：" << len << "字节" << endl;
        w_file.write((const char *)buffer, len);
    }

    header.data_size = pcmBytes;
    header.file_size = header.data_size + sizeof(header) - 8;
    w_file.seekp(4, std::ios::beg);
    w_file.write(reinterpret_cast<const char *>(&header.file_size), 4);
    w_file.seekp(40, std::ios::beg);
    w_file.write(reinterpret_cast<const char *>(&header.data_size), 4);
    w_file.close();
    // 同时也要退出程序
    exit(0);
}