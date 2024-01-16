#include "SonosChannel.h"

SonosChannel::SonosChannel(SonosApi& sonosApi)
    : _sonosApi(sonosApi), _groupVolumeController(true)
{
    auto parameterIP = (uint32_t)ParamSON_CHSonosIPAddress;
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
    if (volume != (uint8_t)KoSON_CHVolumeState.value(DPT_Scaling))
        KoSON_CHVolumeState.value(volume, DPT_Scaling);
}

void SonosChannel::processInputKo(GroupObject& ko)
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
    if (cmd == "getuuid")
    {
        Serial.println();
        Serial.println(_sonosApi.getLocalUID().c_str());
    }
    else if (cmd.rfind("setvol ", 0) == 0)
    {
        Serial.println();
        int value = atoi(cmd.c_str() + 7);
        if (value < 0 || value > 100)
            Serial.printf("Invalid volume %d\r\n", value);
        else
            _volumeController.setVolume(value);
    }
    else if (cmd.rfind("setgroupvol ", 0) == 0)
    {
        Serial.println();
        int value = atoi(cmd.c_str() + 12);
        if (value < 0 || value > 100)
            Serial.printf("Invalid volume %d\r\n", value);
        else
            _volumeController.setVolume(value);
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
    else if (cmd == "getplaystate")
    {
        Serial.println();
        Serial.println(_sonosApi.getPlayState());
    }
    else
        return false;
    return true;
}
