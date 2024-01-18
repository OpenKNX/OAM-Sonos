#include "SonosChannel.h"

SonosChannel::SonosChannel(uint8_t _channelIndex /* this parameter is used in macros, do not rename */, SonosApi& sonosApi)
    : _sonosApi(sonosApi), _groupVolumeController(true), _speakerIP(), _name()
{
    this->_channelIndex = _channelIndex;
    auto parameterIP = (uint32_t)ParamSON_CHSonosIPAddress;
    uint32_t arduinoIP = ((parameterIP & 0xFF000000) >> 24) | ((parameterIP & 0x00FF0000) >> 8) | ((parameterIP & 0x0000FF00) << 8) | ((parameterIP & 0x000000FF) << 24);
    _speakerIP = arduinoIP;
    _name = _speakerIP.toString();
    _sonosApi.setCallback(this);
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

void SonosChannel::notificationGroupVolumeChanged(uint8_t volume)
{
    if (volume != (uint8_t)KoSON_CHGroupVolumeState.value(DPT_Scaling))
        KoSON_CHGroupVolumeState.value(volume, DPT_Scaling);
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
    if (cmd == "uid")
    {
        Serial.println();
        Serial.println(_sonosApi.getUID().c_str());
    }
    else if (cmd == "track")
    {
        Serial.println();
        auto trackInfo = _sonosApi.getTrackInfo();
        Serial.println(trackInfo->queueIndex);
        Serial.println(trackInfo->duration);
        Serial.println(trackInfo->position);
        Serial.println(trackInfo->uri);
        Serial.println(trackInfo->metadata);
    }
    else if (cmd.rfind("vol ", 0) == 0)
    {
        Serial.println();
        int value = atoi(cmd.c_str() + 4);
        if (value < 0 || value > 100)
            Serial.printf("Invalid volume %d\r\n", value);
        else
            _volumeController.setVolume(value);
    }
    else if (cmd.rfind("gvol ", 0) == 0)
    {
        Serial.println();
        int value = atoi(cmd.c_str() + 5);
        if (value < 0 || value > 100)
            Serial.printf("Invalid volume %d\r\n", value);
        else
            _volumeController.setVolume(value);
    }
    else if (cmd == "vol")
    {
        Serial.println();
        Serial.println(_sonosApi.getVolume());
    }
    else if (cmd == "gvol")
    {
        Serial.println();
        Serial.println(_sonosApi.getGroupVolume());
    }
    else if (cmd == "state")
    {
        Serial.println();
        Serial.println(_sonosApi.getPlayState());
    }
    else if (cmd == "play")
    {
        Serial.println();
        _sonosApi.play();
    }
    else if (cmd == "pause")
    {
        Serial.println();
        _sonosApi.pause();
    }
    else if (cmd == "next")
    {
        Serial.println();
        _sonosApi.next();
    }
    else if (cmd == "prev")
    {
        Serial.println();
        _sonosApi.previous();
    }
    else
        return false;
    return true;
}
