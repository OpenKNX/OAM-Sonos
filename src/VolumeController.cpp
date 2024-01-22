#include "VolumeController.h"

VolumeController::VolumeController(bool groupVolume)
 : _groupVolume(groupVolume)
 {
 }

void VolumeController::loop1(SonosApi& sonosApi, IPAddress speakerIP, uint8_t _channelIndex)
{

    if (_targetVolume != 255)
    {
        auto volume = _targetVolume;
        _targetVolume = 255;
        if (_groupVolume)
        {
            auto groupCoordinator = sonosApi.findGroupCoordinator();
            if (groupCoordinator != nullptr)
                groupCoordinator->setGroupVolume(volume);
        }
        else
            sonosApi.setVolume(volume);
    }
}

void VolumeController::setVolume(uint8_t volume)
{
    _targetVolume = volume;
}
