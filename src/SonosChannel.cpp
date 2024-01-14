#include "SonosChannel.h"

SonosChannel::SonosChannel(SonosApi& sonosApi)
    : _sonosApi(sonosApi)
{
    auto parameterIP = (uint32_t) ParamSON_CHSonosIPAddress;
    uint32_t arduinoIP = ((parameterIP & 0xFF000000) >> 24) | ((parameterIP & 0x00FF0000) >> 8) | ((parameterIP & 0x0000FF00) << 8) | ((parameterIP & 0x000000FF) << 24); 
    _speakerIP = arduinoIP;
    _name = _speakerIP.toString();
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
    _volumeController.loop1(_sonosApi, _speakerIP, _channelIndex);
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
    }
}

bool SonosChannel::processCommand(const std::string cmd, bool diagnoseKo)
{
    if (cmd == "setvol")
    {
        Serial.println();
        _volumeController.setVolume(3);
    }
    else if (cmd == "subscribe")
    {
        Serial.println();
        _sonosApi.subscribeAVTransport(_speakerIP);
    }
    else
        return false;
    return true;
}
