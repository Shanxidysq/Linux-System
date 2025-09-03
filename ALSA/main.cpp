#include <iostream>
#include "alsa_capture.hpp"
using namespace ox;

int main()
{
    ox::AlsaCapture cap("hw:0,0",44100);
    
}