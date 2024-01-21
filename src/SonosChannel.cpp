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

void SonosChannel::notificationVolumeChanged(SonosApi& caller, uint8_t volume)
{
    if (volume != (uint8_t)KoSON_CHVolumeState.value(DPT_Scaling))
        KoSON_CHVolumeState.value(volume, DPT_Scaling);
}

void SonosChannel::notificationMuteChanged(SonosApi& caller, boolean mute)
{
    if (mute != (boolean)KoSON_CHMuteState.value(DPT_Switch))
        KoSON_CHMuteState.value(mute, DPT_Switch);
}

void SonosChannel::notificationGroupVolumeChanged(SonosApi& caller, uint8_t volume)
{
    if (volume != (uint8_t)KoSON_CHGroupVolumeState.value(DPT_Scaling))
        KoSON_CHGroupVolumeState.value(volume, DPT_Scaling);
}

void SonosChannel::notificationGroupMuteChanged(SonosApi& caller, boolean mute)
{
    if (mute != (boolean)KoSON_CHGroupMuteState.value(DPT_Switch))
        KoSON_CHGroupMuteState.value(mute, DPT_Switch);
}

void SonosChannel::notificationPlayStateChanged(SonosApi& caller, SonosApiPlayState playState)
{
    bool playing = playState == SonosApiPlayState::Playing || playState == SonosApiPlayState::Transitioning;
    if (playing != (boolean)KoSON_CHPlayFeedback.value(DPT_Start))
        KoSON_CHPlayFeedback.value(playing, DPT_Start);
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
        case SON_KoCHVolumeRelativ:
        {
            bool increase = ko.value(DPT_Step);
            auto volume = increase ? (int8_t) ParamSON_CHRelativVolumeStep : -(int8_t) ParamSON_CHRelativVolumeStep;
            logDebugP("Set volume relative %d", volume);
            _sonosApi.setVolumeRelative(volume);
            break;
        }
        case SON_KoCHMute:
        {
            boolean mute = ko.value(DPT_Switch);
            logDebugP("Set mute %d", mute);
            _sonosApi.setMute(mute);
            break;            
        }
        case SON_KoCHGroupVolume:
        {
            uint8_t volume = ko.value(DPT_Scaling);
            logDebugP("Set group volume %d", volume);
            _groupVolumeController.setVolume(volume);
            break;
        }
        case SON_KoCHGroupVolumeRelativ:
        {
            bool increase = ko.value(DPT_Step);
            auto volume = increase ? (int8_t) ParamSON_CHGroupRelativVolumeStep : -(int8_t) ParamSON_CHGroupRelativVolumeStep;
            logDebugP("Set volume relative %d", volume);
            _sonosApi.setGroupVolumeRelative(volume);
            break;
        }
        case SON_KoCHGroupMute:
        {
            boolean mute = ko.value(DPT_Switch);
            logDebugP("Set group mute %d", mute);
            _sonosApi.setGroupMute(mute);
            break;
        }
        case SON_KoCHPlay:
        {
            boolean play = ko.value(DPT_Switch);
            logDebugP("Set play %d", play);
            if (play)
                _sonosApi.play();
            else
                _sonosApi.pause();
            break;
        }
        case SON_KoCHPreviousNext:
        {
            boolean next = ko.value(DPT_UpDown);
            if (next)
            {
                logDebugP("Set Next");
                _sonosApi.next();
            }
            else
            {
                logDebugP("Set Previous");
                _sonosApi.previous();
            }
            break;
        }
        case SON_KoCHJoinNextActiveGroup:
        {
            boolean trigger = ko.value(DPT_Trigger);
            if (trigger)
            {
                logDebugP("Join next playing group");
                joinNextPlayingGroup();
            }
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
        Serial.println(trackInfo.queueIndex);
        Serial.println(trackInfo.duration);
        Serial.println(trackInfo.position);
        Serial.println(trackInfo.uri);
        Serial.println(trackInfo.metadata);
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
    else if (cmd.rfind("rvol ", 0) == 0)
    {
        Serial.println();
        int value = atoi(cmd.c_str() + 5);
        if (value < -100 || value > 100)
            Serial.printf("Invalid relative volume %d\r\n", value);
        else
            _sonosApi.setVolumeRelative(value);
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
    else if (cmd.rfind("rgvol ", 0) == 0)
    {
        Serial.println();
        int value = atoi(cmd.c_str() + 6);
        if (value < -100 || value > 100)
            Serial.printf("Invalid relative volume %d\r\n", value);
        else
            _sonosApi.setGroupVolumeRelative(value);
    }
    else if (cmd.rfind("mute ", 0) == 0)
    {
        Serial.println();
        bool mute = cmd.substr(5) == "1";
        _sonosApi.setMute(mute);
    }
    else if (cmd.rfind("gmute ", 0) == 0)
    {
        Serial.println();
        bool mute = cmd.substr(6) == "1";
        _sonosApi.setGroupMute(mute);
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
    else if (cmd == "mute")
    {
        Serial.println();
        Serial.println(_sonosApi.getMute() ? "1" : "0");
    }
    else if (cmd == "gmute")
    {
        Serial.println();
        Serial.println(_sonosApi.getGroupMute() ? "1" : "0");
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
    else if (cmd == "findc")
    {
        Serial.println();
        auto groupCoordinator = _sonosApi.findGroupCoordinator();
        if (groupCoordinator != nullptr)
            Serial.println(groupCoordinator->getSpeakerIP().toString());
        else
            Serial.println("Not found");
    }
    else if (cmd == "findnpc")
    {
        Serial.println();
        auto groupCoordinator = _sonosApi.findNextPlayingGroupCoordinator();
        if (groupCoordinator != nullptr)
            Serial.println(groupCoordinator->getSpeakerIP().toString());
        else
            Serial.println("Not found");
    }
    else if (cmd == "joinnext")
    {
       Serial.println();
       joinNextPlayingGroup();
    }
    else
        return false;
    return true;
}

void SonosChannel::joinNextPlayingGroup()
{
    auto groupCoordinator = _sonosApi.findNextPlayingGroupCoordinator();
    if (groupCoordinator != nullptr)
    {
         logDebugP("Join with %s", groupCoordinator->getSpeakerIP().toString().c_str());
        _sonosApi.joinToGroupCoordinator(groupCoordinator);
    }
    else
        logDebugP("No next playing group found");
}
