#include "SonosChannel.h"
#include "SonosModule.h"

#if ARDUINO_ARCH_ESP32
#include "WebSocketsClient.h"
#endif
#include "WiFiClientSecure.h"

SonosChannel::SonosChannel(SonosModule& sonosModule, uint8_t _channelIndex /* this parameter is used in macros, do not rename */, SonosApi& sonosApi)
    : _sonosModule(sonosModule), _name()
{
    this->_channelIndex = _channelIndex;
    auto parameterIP = (uint32_t)ParamSON_CHSonosIPAddress;
    uint32_t arduinoIP = ((parameterIP & 0xFF000000) >> 24) | ((parameterIP & 0x00FF0000) >> 8) | ((parameterIP & 0x0000FF00) << 8) | ((parameterIP & 0x000000FF) << 24);
    IPAddress speakerIP = IPAddress(arduinoIP);
    _sonosSpeaker = sonosApi.addSpeaker(speakerIP);
    _name = speakerIP.toString();
#if USE_ESP_ASNC_WEB_SERVER
    _sonosSpeaker->setCallback(this);
#endif
}

const IPAddress SonosChannel::speakerIP()
{
    return _sonosSpeaker->getSpeakerIP();
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
#ifdef ARDUINO_ARCH_ESP32   
    if (_playNotification != nullptr)
    {
        if (_playNotification->checkFinished())
        {
            delete _playNotification;
            _playNotification = nullptr;
        }
    }
#endif
}

void SonosChannel::notificationVolumeChanged(SonosSpeaker* speaker, uint8_t volume)
{
    if (volume != (uint8_t)KoSON_CHVolumeState.value(DPT_Scaling))
        KoSON_CHVolumeState.value(volume, DPT_Scaling);
}

void SonosChannel::notificationMuteChanged(SonosSpeaker* speaker, boolean mute)
{
    if (mute != (boolean)KoSON_CHMuteState.value(DPT_Switch))
        KoSON_CHMuteState.value(mute, DPT_Switch);
}

void SonosChannel::notificationGroupVolumeChanged(SonosSpeaker* speaker, uint8_t volume)
{
    if (volume != (uint8_t)KoSON_CHGroupVolumeState.value(DPT_Scaling))
        KoSON_CHGroupVolumeState.value(volume, DPT_Scaling);
    if (_sonosSpeaker == speaker)
    {
        // this channel is group coordinator, notify all participants
        for (int channelIndex = 0; channelIndex < _sonosModule.getNumberOfChannels(); channelIndex++)
        {
            if (channelIndex == _channelIndex)
                continue;
            auto channel = (SonosChannel*)_sonosModule.getChannel(channelIndex);
            if (channel != nullptr && channel->_sonosSpeaker->findGroupCoordinator(true) == speaker)
            {
                channel->notificationGroupVolumeChanged(speaker, volume);
            }
        }
    }
}

void SonosChannel::notificationGroupMuteChanged(SonosSpeaker* speaker, boolean mute)
{
    if (mute != (boolean)KoSON_CHGroupMuteState.value(DPT_Switch))
        KoSON_CHGroupMuteState.value(mute, DPT_Switch);
    if (_sonosSpeaker == speaker)
    {
        // this channel is group coordinator, notify all participants
        for (int channelIndex = 0; channelIndex < _sonosModule.getNumberOfChannels(); channelIndex++)
        {
            if (channelIndex == _channelIndex)
                continue;
            auto channel = (SonosChannel*)_sonosModule.getChannel(channelIndex);
            if (channel != nullptr && channel->_sonosSpeaker->findGroupCoordinator(true) == speaker)
            {
                channel->notificationGroupMuteChanged(speaker, mute);
            }
        }
    }
}

void SonosChannel::notificationPlayStateChanged(SonosSpeaker* speaker, SonosApiPlayState playState)
{
    if (_sonosSpeaker->findGroupCoordinator() == _sonosSpeaker)
    {
        // this channel is group coordinator
        bool playing = playState == SonosApiPlayState::Playing || playState == SonosApiPlayState::Transitioning;
        if (playing != (boolean)KoSON_CHPlayFeedback.value(DPT_Start))
            KoSON_CHPlayFeedback.value(playing, DPT_Start);

        // notify all participants
        for (int channelIndex = 0; channelIndex < _sonosModule.getNumberOfChannels(); channelIndex++)
        {
            if (channelIndex == _channelIndex)
                continue;
            auto channel = (SonosChannel*)_sonosModule.getChannel(channelIndex);
            if (channel != nullptr && channel->_sonosSpeaker->findGroupCoordinator(true) == speaker)
            {
                channel->notificationPlayStateChanged(speaker, playState);
            }
        }
    }
    else if (speaker != _sonosSpeaker)
    {
        // called from master, take over play state
        bool playing = playState == SonosApiPlayState::Playing || playState == SonosApiPlayState::Transitioning;
        if (playing != (boolean)KoSON_CHPlayFeedback.value(DPT_Start))
            KoSON_CHPlayFeedback.value(playing, DPT_Start);
    }
}

void SonosChannel::notificationGroupCoordinatorChanged(SonosSpeaker* speaker)
{
    auto groupCoordinator = _sonosSpeaker->findGroupCoordinator();
    if (groupCoordinator == nullptr)
        return;
    auto playState = groupCoordinator->getPlayState();
    bool playing = playState == SonosApiPlayState::Playing || playState == SonosApiPlayState::Transitioning;
    if (playing != (boolean)KoSON_CHPlayFeedback.value(DPT_Start))
        KoSON_CHPlayFeedback.value(playing, DPT_Start);
    auto groupVolume = groupCoordinator->getGroupVolume();
    if (groupVolume != (uint8_t)KoSON_CHGroupVolumeState.value(DPT_Scaling))
        KoSON_CHGroupVolumeState.value(groupVolume, DPT_Scaling);
    auto groupMute = groupCoordinator->getGroupMute();
    if (groupMute != (boolean)KoSON_CHGroupMuteState.value(DPT_Switch))
        KoSON_CHGroupMuteState.value(groupMute, DPT_Switch);
}

void SonosChannel::notificationTrackChanged(SonosSpeaker* speaker, SonosTrackInfo& trackInfo)
{
    uint8_t sourceNumber = 0;
    if (trackInfo.uri.length() != 0)
    {
        for (uint8_t _channelIndex = 0; _channelIndex < SONSRC_ChannelCount && sourceNumber == 0; _channelIndex++)
        {
            auto sourceType = ParamSONSRC_CHSourceType;
            switch (sourceType)
            {
                case 1: // Radio
                {
                    if (trackInfo.uri.startsWith(SonosApi::DefaultSchemaInternetRadio))
                    {
                        const char* uri = (const char*)ParamSONSRC_CHSourceUri;
                        if (strcmp(trackInfo.uri.c_str() + strlen(SonosApi::DefaultSchemaInternetRadio), uri) == 0)
                            sourceNumber = _channelIndex + 1;
                    }
                    break;
                }
                case 2: // Http
                {
                    const char* uri = (const char*)ParamSONSRC_CHSourceUri;
                    if (strcmp(trackInfo.uri.c_str(), uri) == 0)
                        sourceNumber = _channelIndex + 1;
                    break;
                }
                case 3: // Music libary file
                {
                    if (trackInfo.uri.startsWith(SonosApi::SchemaMusicLibraryFile))
                    {
                        String uri = (const char*)ParamSONSRC_CHSourceUri;
                        if (strcmp(trackInfo.uri.c_str() + strlen(SonosApi::SchemaMusicLibraryFile), uri.c_str()) == 0)
                            sourceNumber = _channelIndex + 1;
                    }
                    break;
                }
                case 4: // Music libary dir
                {
                    if (trackInfo.uri.startsWith(SonosApi::SchemaMusicLibraryFile))
                    {
                        String uri = (const char*)ParamSONSRC_CHSourceUri;
                        uri.replace(" ", "%20");
                        if (!uri.endsWith("/"))
                            uri += "/";
                        if (strncmp(trackInfo.uri.c_str() + strlen(SonosApi::SchemaMusicLibraryFile), uri.c_str(), uri.length()) == 0)
                            sourceNumber = _channelIndex + 1;
                    }
                    break;
                }
                case 5: // Line In
                {
                    if (trackInfo.uri.startsWith(SonosApi::SchemaLineIn))
                    {
                        auto groupCoordinator = _sonosSpeaker->findGroupCoordinator();
                        if (groupCoordinator != nullptr &&
                            strcmp(trackInfo.uri.c_str() + strlen(SonosApi::SchemaLineIn), groupCoordinator->getUID().c_str()) == 0)
                            sourceNumber = _channelIndex + 1;
                    }
                    break;
                }
                case 6: // TV In
                {
                    if (trackInfo.uri.startsWith(SonosApi::SchemaTVIn))
                    {
                        auto groupCoordinator = _sonosSpeaker->findGroupCoordinator();
                        if (groupCoordinator != nullptr)
                        {
                            auto url = groupCoordinator->getUID() + SonosApi::UrlPostfixTVIn;
                            if (strcmp(trackInfo.uri.c_str() + strlen(SonosApi::SchemaTVIn), url.c_str()) == 0)
                                sourceNumber = _channelIndex + 1;
                        }
                    }
                    break;
                }
                case 7: // Sonos playlist
                {
                    // Can not be detected
                }
                case 8: // Sonos Uri
                {
                    const char* uri = (const char*)ParamSONSRC_CHSourceUri;
                    if (trackInfo.uri == uri)
                        sourceNumber = _channelIndex + 1;
                }
            }
        }
    }
    Serial.print("Source State: ");
    Serial.println(sourceNumber);
    if ((uint8_t)KoSON_CHSourceState.value(DPT_Value_1_Ucount) != sourceNumber)
    {
        KoSON_CHSourceState.value(sourceNumber, DPT_Value_1_Ucount);
    }
}

void SonosChannel::processInputKo(GroupObject& ko)
{
    auto index = SON_KoCalcIndex(ko.asap());
    switch (index)
    {
        case SON_KoCHVolume:
        {
            uint8_t volume = ko.value(DPT_Scaling);
            logDebugP("Set volume %d", volume);
            _sonosSpeaker->setVolume(volume);
            break;
        }
        case SON_KoCHVolumeRelativ:
        {
            bool increase = ko.value(DPT_Step);
            auto volume = increase ? (int8_t)ParamSON_CHRelativVolumeStep : -(int8_t)ParamSON_CHRelativVolumeStep;
            logDebugP("Set volume relative %d", volume);
            _sonosSpeaker->setVolumeRelative(volume);
            break;
        }
        case SON_KoCHMute:
        {
            boolean mute = ko.value(DPT_Switch);
            logDebugP("Set mute %d", mute);
            _sonosSpeaker->setMute(mute);
            break;
        }
        case SON_KoCHGroupVolume:
        {
            uint8_t volume = ko.value(DPT_Scaling);
            logDebugP("Set group volume %d", volume);
            _sonosSpeaker->setGroupVolume(volume);
            break;
        }
        case SON_KoCHGroupVolumeRelativ:
        {
            bool increase = ko.value(DPT_Step);
            auto volume = increase ? (int8_t)ParamSON_CHGroupRelativVolumeStep : -(int8_t)ParamSON_CHGroupRelativVolumeStep;
            logDebugP("Set volume relative %d", volume);
            auto groupCoordinator = _sonosSpeaker->findGroupCoordinator();
            if (groupCoordinator != nullptr)
                groupCoordinator->setGroupVolumeRelative(volume);
            break;
        }
        case SON_KoCHGroupMute:
        {
            boolean mute = ko.value(DPT_Switch);
            logDebugP("Set group mute %d", mute);
            auto groupCoordinator = _sonosSpeaker->findGroupCoordinator();
            if (groupCoordinator != nullptr)
                groupCoordinator->setGroupMute(mute);
            break;
        }
        case SON_KoCHPlay:
        {
            boolean play = ko.value(DPT_Switch);
            logDebugP("Set play %d", play);
            auto groupCoordinator = _sonosSpeaker->findGroupCoordinator();
            if (groupCoordinator == nullptr)
                return;
            if (play)
                groupCoordinator->play();
            else
                groupCoordinator->pause();
            break;
        }
        case SON_KoCHPreviousNext:
        {
            boolean next = ko.value(DPT_UpDown);
            if (next)
            {
                logDebugP("Set Next");
                _sonosSpeaker->next();
            }
            else
            {
                logDebugP("Set Previous");
                _sonosSpeaker->previous();
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
        case SON_KoCHJoinChannelNumber:
        {
            uint8_t channelNumber = ko.value(DPT_Value_1_Ucount);
            logDebugP("Join channel %d", channelNumber);
            joinChannel(channelNumber);
        }
        case SON_KoCHSourceNumber:
        {
            uint8_t sourceNumber = ko.value(DPT_Value_1_Ucount);
            if (sourceNumber < 1 || sourceNumber > SONSRC_ChannelCount)
                return;
            uint8_t _channelIndex = sourceNumber - 1;
            auto sourceType = ParamSONSRC_CHSourceType;
            auto groupCoordinator = _sonosSpeaker->findGroupCoordinator();
            if (groupCoordinator == nullptr)
                return;
            switch (sourceType)
            {
                case 1: // Radio
                {
                    const char* uri = (const char*)ParamSONSRC_CHSourceUri;
                    const char* title = (const char*)ParamSONSRC_CHSourceTitle;
                    const char* imageUrl = (const char*)ParamSONSRC_CHSourceUriImage;
                    groupCoordinator->playInternetRadio(uri, title, imageUrl);
                    break;
                }
                case 2: // Http
                {
                    const char* uri = (const char*)ParamSONSRC_CHSourceUri;
                    groupCoordinator->playFromHttp(uri);
                    break;
                }
                case 3: // Music libary file
                {
                    const char* uri = (const char*)ParamSONSRC_CHSourceUri;
                    groupCoordinator->playMusicLibraryFile(uri);
                    break;
                }
                case 4: // Music libary dir
                {
                    const char* uri = (const char*)ParamSONSRC_CHSourceUri;
                    groupCoordinator->setShuffle(ParamSONSRC_CHRandom);
                    groupCoordinator->playMusicLibraryDirectory(uri);
                    break;
                }
                case 5: // Line In
                {
                    if (groupCoordinator != _sonosSpeaker)
                    {
                        groupCoordinator->delegateGroupCoordinationTo(_sonosSpeaker, false);
                    }
                    _sonosSpeaker->playLineIn();
                    break;
                }
                case 6: // TV In
                {
                    if (groupCoordinator != _sonosSpeaker)
                    {
                        groupCoordinator->delegateGroupCoordinationTo(_sonosSpeaker, false);
                    }
                    _sonosSpeaker->playTVIn();
                    break;
                }
                case 7: // Sonos playlist
                {
                    const char* uri = (const char*)ParamSONSRC_CHSourceUri;
                    groupCoordinator->setShuffle(ParamSONSRC_CHRandom);
                    groupCoordinator->playSonosPlaylist(uri);
                }
                case 8: // Sonos Uri
                {
                    const char* uri = (const char*)ParamSONSRC_CHSourceUri;
                    const char* title = (const char*)ParamSONSRC_CHSourceTitle;
                    const char* imageUrl = (const char*)ParamSONSRC_CHSourceUriImage;
                    if (strlen(title) > 0 || strlen(imageUrl) > 0)
                        groupCoordinator->playInternetRadio(uri, title, imageUrl, "");
                    else
                    {
                        groupCoordinator->setShuffle(ParamSONSRC_CHRandom);
                        groupCoordinator->setAVTransportURI(nullptr, uri);     
                        groupCoordinator->play();
                    }
                    break;
                }
            }
        }
        case SON_KoCHNotificationSound1:
        case SON_KoCHNotificationSound2:
        case SON_KoCHNotificationSound3:
        case SON_KoCHNotificationSound4:
        {
            boolean trigger = ko.value(DPT_Trigger);
            if (trigger)
            {
                byte notification = index - SON_KoCHNotificationSound1 + 1;
                logDebugP("play notification %d", notification);      
#if ARDUINO_ARCH_ESP32    
                playNotification(notification);
#endif
            }
        }
    }
}

void SonosChannel::joinChannel(uint8_t channelNumber)
{
    if (channelNumber > 0 && channelNumber <= _sonosModule.getNumberOfChannels())
    {
        auto sonosChannel = (SonosChannel*)_sonosModule.getChannel(channelNumber - 1);
        if (sonosChannel != nullptr)
        {
            if (sonosChannel == this)
            {
                channelNumber = 0;
            }
            else
            {
                auto groupCoordinator = sonosChannel->_sonosSpeaker->findGroupCoordinator();
                _sonosSpeaker->joinToGroupCoordinator(groupCoordinator);
            }
        }
    }
    if (channelNumber == 0)
    {
        if (!delegateCoordination(false))
            _sonosSpeaker->unjoin();
    }
}

bool SonosChannel::processCommand(const std::string cmd, bool diagnoseKo)
{
    if (cmd == "uid")
    {
        Serial.println();
        Serial.println(_sonosSpeaker->getUID().c_str());
    }
    else if (cmd == "track")
    {
        Serial.println();
        auto trackInfo = _sonosSpeaker->getTrackInfo();
        Serial.println(trackInfo.trackNumber);
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
            _sonosSpeaker->setVolume(value);
    }
    else if (cmd.rfind("rvol ", 0) == 0)
    {
        Serial.println();
        int value = atoi(cmd.c_str() + 5);
        if (value < -100 || value > 100)
            Serial.printf("Invalid relative volume %d\r\n", value);
        else
            _sonosSpeaker->setVolumeRelative(value);
    }
    else if (cmd.rfind("gvol ", 0) == 0)
    {
        Serial.println();
        int value = atoi(cmd.c_str() + 5);
        if (value < 0 || value > 100)
            Serial.printf("Invalid volume %d\r\n", value);
        else
            _sonosSpeaker->setGroupVolume(value);
    }
    else if (cmd.rfind("rgvol ", 0) == 0)
    {
        Serial.println();
        int value = atoi(cmd.c_str() + 6);
        if (value < -100 || value > 100)
            Serial.printf("Invalid relative volume %d\r\n", value);
        else
            _sonosSpeaker->setGroupVolumeRelative(value);
    }
    else if (cmd.rfind("treb ", 0) == 0)
    {
        Serial.println();
        int value = atoi(cmd.c_str() + 5);
        if (value < -10 || value > 10)
            Serial.printf("Invalid treble %d\r\n", value);
        else
            _sonosSpeaker->setTreble(value);
    }
    else if (cmd == "treb")
    {
        Serial.println();
        Serial.println(_sonosSpeaker->getTreble());
    }
    else if (cmd.rfind("bass ", 0) == 0)
    {
        Serial.println();
        int value = atoi(cmd.c_str() + 5);
        if (value < -10 || value > 10)
            Serial.printf("Invalid bass %d\r\n", value);
        else
            _sonosSpeaker->setBass(value);
    }
    else if (cmd == "bass")
    {
        Serial.println();
        Serial.println(_sonosSpeaker->getBass());
    }
    else if (cmd.rfind("mute ", 0) == 0)
    {
        Serial.println();
        bool mute = cmd.substr(5) == "1";
        _sonosSpeaker->setMute(mute);
    }
    else if (cmd.rfind("gmute ", 0) == 0)
    {
        Serial.println();
        bool mute = cmd.substr(6) == "1";
        _sonosSpeaker->setGroupMute(mute);
    }
    else if (cmd == "vol")
    {
        Serial.println();
        Serial.println(_sonosSpeaker->getVolume());
    }
    else if (cmd == "gvol")
    {
        Serial.println();
        Serial.println(_sonosSpeaker->getGroupVolume());
    }
    else if (cmd.rfind("ldn ", 0) == 0)
    {
        Serial.println();
        bool loudness = cmd.substr(4) == "1";
        _sonosSpeaker->setLoudness(loudness);
    }
    else if (cmd == "ldn")
    {
        Serial.println();
        Serial.println(_sonosSpeaker->getLoudness() ? "1" : "0");
    }
    else if (cmd.rfind("led ", 0) == 0)
    {
        Serial.println();
        bool on = cmd.substr(4) == "1";
        _sonosSpeaker->setStatusLight(on);
    }
    else if (cmd == "led")
    {
        Serial.println();
        Serial.println(_sonosSpeaker->getStatusLight() ? "1" : "0");
    }
    else if (cmd == "mute")
    {
        Serial.println();
        Serial.println(_sonosSpeaker->getMute() ? "1" : "0");
    }
    else if (cmd == "gmute")
    {
        Serial.println();
        Serial.println(_sonosSpeaker->getGroupMute() ? "1" : "0");
    }
    else if (cmd == "state")
    {
        Serial.println();
        Serial.println(_sonosSpeaker->getPlayState());
    }
    else if (cmd == "play")
    {
        Serial.println();
        _sonosSpeaker->play();
    }
    else if (cmd == "pause")
    {
        Serial.println();
        _sonosSpeaker->pause();
    }
    else if (cmd == "next")
    {
        Serial.println();
        _sonosSpeaker->next();
    }
    else if (cmd == "prev")
    {
        Serial.println();
        _sonosSpeaker->previous();
    }
    else if (cmd == "findc")
    {
        Serial.println();
        auto groupCoordinator = _sonosSpeaker->findGroupCoordinator();
        if (groupCoordinator != nullptr)
            Serial.println(groupCoordinator->getSpeakerIP().toString());
        else
            Serial.println("Not found");
    }
    else if (cmd == "findnpc")
    {
        Serial.println();
        auto groupCoordinator = _sonosSpeaker->findNextPlayingGroupCoordinator();
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
    else if (cmd.rfind("dele ", 0) == 0)
    {
        Serial.println();
        auto targetChannel = atoi(cmd.substr(5).c_str());
        if (targetChannel < 1 || targetChannel > _sonosModule.getNumberOfChannels())
        {
            Serial.print("Invalid channel ");
            Serial.println(targetChannel);
            return true;
        }
        if (targetChannel - 1 == _channelIndex)
        {
            Serial.println("Target channel must be a differnt channel");
            return true;
        }
        auto channel = (SonosChannel*)_sonosModule.getChannel(targetChannel - 1);
        if (channel == nullptr)
        {
            Serial.print("Channel ");
            Serial.print(targetChannel);
            Serial.println("is deactivated");
            return true;
        }
        _sonosSpeaker->delegateGroupCoordinationTo(channel->_sonosSpeaker, true);
    }
    else if (cmd.rfind("join ", 0) == 0)
    {
        Serial.println();
        auto targetChannel = atoi(cmd.substr(5).c_str());
        joinChannel(targetChannel);
    }
#if ARDUINO_ARCH_ESP32 
    else if (cmd.rfind("noti ", 0) == 0)
    {
        Serial.println();
        auto notificationNr = atoi(cmd.substr(5).c_str());
        playNotification(notificationNr);
    }
#endif
    else if (cmd.rfind("src ", 0) == 0)
    {
        Serial.println();
        auto src = atoi(cmd.substr(4).c_str());
        auto ko = KoSON_CHSourceNumber;
        ko.valueNoSend((uint8_t) src, DPT_Value_1_Ucount);
        processInputKo(ko);
    }
    else if (cmd.rfind("pl ", 0) == 0)
    {
        auto playList = cmd.substr(2);
        _sonosSpeaker->playSonosPlaylist(playList.c_str());
    }
    else if (cmd == "test1")
    {
        _sonosSpeaker->playInternetRadio("https://orf-live.ors-shoutcast.at/wie-q2a.m3u", "Radio Wien");
    }
    else if (cmd == "test2")
    {
        _sonosSpeaker->playMusicLibraryDirectory("//192.168.0.1/Share/Storage/Musik/Violent Femmes/3");
    }
    else if (cmd == "test3")
    {
        _sonosSpeaker->playMusicLibraryDirectory("//192.168.0.1/Share/Storage/Musik/Whippersnapper/Stories/");
    }
    else if (cmd == "test4")
    {
        _sonosSpeaker->playMusicLibraryFile("//192.168.0.1/Share/Storage/Musik/Violent%20Femmes/3/01%20-%20Violent%20Femmes%20-%20Nightmares.mp3");
    }
    else if (cmd == "test5")
    {
        _sonosSpeaker->stop();
    }
    else
        return false;
    return true;
}
#if ARDUINO_ARCH_ESP32 
void SonosChannel::playNotification(byte notificationNumber)
{
    if (_playNotification != nullptr)
    {
        delete _playNotification;
        _playNotification = nullptr;
    }
    switch (notificationNumber)
    {
        case 1:
            _playNotification = new SonosApiPlayNotification(_sonosSpeaker->getSpeakerIP(), (const char*)ParamSON_NotificationUrl1, ParamSON_NotificationVolume1, _sonosSpeaker->getUID());
            break;
        case 2:
            _playNotification = new SonosApiPlayNotification(_sonosSpeaker->getSpeakerIP(), (const char*)ParamSON_NotificationUrl2, ParamSON_NotificationVolume2, _sonosSpeaker->getUID());
            break;
        case 3:
            _playNotification = new SonosApiPlayNotification(_sonosSpeaker->getSpeakerIP(), (const char*)ParamSON_NotificationUrl3, ParamSON_NotificationVolume3, _sonosSpeaker->getUID());
            break;
        case 4:
            _playNotification = new SonosApiPlayNotification(_sonosSpeaker->getSpeakerIP(), (const char*)ParamSON_NotificationUrl4, ParamSON_NotificationVolume4, _sonosSpeaker->getUID());
            break;
    }
}
#endif

void SonosChannel::joinNextPlayingGroup()
{
    delegateCoordination(true);

    auto groupCoordinator = _sonosSpeaker->findNextPlayingGroupCoordinator();
    if (groupCoordinator != nullptr)
    {
        logDebugP("Join with %s", groupCoordinator->getSpeakerIP().toString().c_str());
        _sonosSpeaker->joinToGroupCoordinator(groupCoordinator);
    }
    else
        logDebugP("No next playing group found");
}

bool SonosChannel::delegateCoordination(bool rejoinGroup)
{
    auto currentGroupCoordinator = _sonosSpeaker->findGroupCoordinator(true);
    if (currentGroupCoordinator == _sonosSpeaker)
    {
        // delegate to other participant of the group
        auto firstParticpant = _sonosSpeaker->findFirstParticipant(true);
        if (firstParticpant != nullptr)
        {
            _sonosSpeaker->delegateGroupCoordinationTo(firstParticpant, rejoinGroup);
            return true;
        }
    }
    return false;
}
