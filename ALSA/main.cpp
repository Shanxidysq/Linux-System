#include <iostream>
#include "alsa_capture.hpp"
using namespace ox;

int main()
{
    // 打开设备
    ox::AlsaCapture cap("hw:0,0",44100);
    // 一般的情况下这里设置完毕参数配置之后再打开设备

    // 打开设备
    cap.Open();
    // 读取一帧的数据
    uint8_t buff[1024]={0};
    int len=0;
    // buff缓冲区大小
    // 缓冲区字节数
    // len每次读取的帧数
    cap.ReadFrame(buff,1024,&len);

}