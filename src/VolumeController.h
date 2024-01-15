#pragma once
#include "Arduino.h"
#include "SonosApi.h"

class VolumeController
{
    volatile uint8_t _targetVolume = 255;
    uint8_t _currentVolume = 255;
    unsigned long _lastVolumeRead = 0;
    bool _groupVolume = false;

    public:
        VolumeController(bool groupVolume = false);
        void loop1(SonosApi& sonosApi, IPAddress speakerIP, uint8_t _channelIndex);
        void setVolume(uint8_t volume);
};