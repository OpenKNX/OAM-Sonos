#include "VolumeController.h"

void VolumeController::loop1(SonosApi& sonosApi, IPAddress speakerIP, uint8_t _channelIndex)
{
    if (_targetVolume != 255)
    {
        sonosApi.setVolume(speakerIP, _targetVolume);
        _targetVolume = 255;
    }
}

void VolumeController::setVolume(uint8_t volume)
{
    _targetVolume = volume;
}
