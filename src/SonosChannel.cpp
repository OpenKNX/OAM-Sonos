#include "SonosChannel.h"

SonosChannel::SonosChannel(SonosApi& sonosApi)
    : _sonosApi(sonosApi), _groupVolumeController(true)
{
    auto parameterIP = (uint32_t) ParamSON_CHSonosIPAddress;
    uint32_t arduinoIP = ((parameterIP & 0xFF000000) >> 24) | ((parameterIP & 0x00FF0000) >> 8) | ((parameterIP & 0x0000FF00) << 8) | ((parameterIP & 0x000000FF) << 24); 
    _speakerIP = arduinoIP;
    _name = _speakerIP.toString();
}

const IPAddress& SonosChannel::speakerIP()
{
    return _speakerIP;
}

const std::string SonosChannel::name() 
{
    return std::string(_name.c_str());
}
const std::string SonosChannel::logPrefix()
{
    return "Sonos.Channel";
}
void SonosChannel::loop1()
{
    _sonosApi.loop();
    _volumeController.loop1(_sonosApi, _speakerIP, _channelIndex);
}

void SonosChannel::notificationVolumeChanged(uint8_t volume) 
{
    if (volume != (uint8_t) KoSON_CHValumeState.value(DPT_Scaling))
        KoSON_CHValumeState.value(volume, DPT_Scaling);
}

void SonosChannel::processInputKo(GroupObject &ko)
{
    switch (SON_KoCalcIndex(ko.asap()))
    {
        case SON_KoCHVolume:
        {
            uint8_t volume = ko.value(DPT_Scaling);
            logDebugP("Set volume %d", volume);
            _volumeController.setVolume(volume);
            break;
        }
        case SON_KoCHGroupVolume:
        {
            uint8_t volume = ko.value(DPT_Scaling);
            logDebugP("Set group volume %d", volume);
            _groupVolumeController.setVolume(volume);
            break;
        }
    }
}

bool SonosChannel::processCommand(const std::string cmd, bool diagnoseKo)
{
    if (cmd == "setvol")
    {
        Serial.println();
        _volumeController.setVolume(3);
    }
    else if (cmd == "setgroupvol")
    {
        Serial.println();
        _volumeController.setVolume(3);
    }
    else if (cmd == "getvol")
    {
        Serial.println();
        Serial.println(_sonosApi.getVolume());
    }
    else if (cmd == "getgroupvol")
    {
        Serial.println();
        Serial.println(_sonosApi.getGroupVolume());
    }
    else
        return false;
    return true;
}
