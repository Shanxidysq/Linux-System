#ifndef __WAV_HPP__
#define __WAV_HPP__
#include <iostream>
namespace ox
{
    class WAVHeader
    {
    public:
        uint8_t riff[4] = {};
        uint32_t file_size = 0;
        uint8_t wave[4] = {};
        uint8_t fmt_id[4] = {};
        uint32_t fmt_size = 0;
        uint16_t format_tag = 0;
        uint16_t channels = 0;
        uint32_t sample_rate = 0;
        uint32_t byte_rate = 0;
        uint16_t block_align = 0;
        uint16_t bits_per_sample = 0;
        uint8_t data_id[4] = {};
        uint32_t data_size = 0;
    };
}
#endif