#include "SonosChannel.h"
#include "SonosModule.h"

#include "WebSocketsClient.h"
#include "WiFiClientSecure.h"

SonosChannel::SonosChannel(SonosModule& sonosModule, uint8_t _channelIndex /* this parameter is used in macros, do not rename */, SonosApi& sonosApi)
    : _sonosModule(sonosModule), _sonosApi(sonosApi), _groupVolumeController(true), _speakerIP(), _name()
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
    if (_playNotification != nullptr)
    {
        if (_playNotification->checkFinished())
        {
            delete _playNotification;
            _playNotification = nullptr;
        }
    }
    _sonosApi.loop();
    _volumeController.loop1(_sonosApi, _speakerIP, _channelIndex);
    _groupVolumeController.loop1(_sonosApi, _speakerIP, _channelIndex);
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
    if (&_sonosApi == &caller)
    {
        // this channel is group coordinator, notify all participants
        for (int channelIndex = 0; channelIndex < _sonosModule.getNumberOfChannels(); channelIndex++)
        {
            if (channelIndex == _channelIndex)
                continue;
            auto channel = (SonosChannel*)_sonosModule.getChannel(channelIndex);
            if (channel != nullptr && channel->_sonosApi.findGroupCoordinator(true) == &caller)
            {
                channel->notificationGroupVolumeChanged(caller, volume);
            }
        }
    }
}

void SonosChannel::notificationGroupMuteChanged(SonosApi& caller, boolean mute)
{
    if (mute != (boolean)KoSON_CHGroupMuteState.value(DPT_Switch))
        KoSON_CHGroupMuteState.value(mute, DPT_Switch);
    if (&_sonosApi == &caller)
    {
        // this channel is group coordinator, notify all participants
        for (int channelIndex = 0; channelIndex < _sonosModule.getNumberOfChannels(); channelIndex++)
        {
            if (channelIndex == _channelIndex)
                continue;
            auto channel = (SonosChannel*)_sonosModule.getChannel(channelIndex);
            if (channel != nullptr && channel->_sonosApi.findGroupCoordinator(true) == &caller)
            {
                channel->notificationGroupMuteChanged(caller, mute);
            }
        }
    }
}

void SonosChannel::notificationPlayStateChanged(SonosApi& caller, SonosApiPlayState playState)
{
    if (_sonosApi.findGroupCoordinator() == &_sonosApi)
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
            if (channel != nullptr && channel->_sonosApi.findGroupCoordinator(true) == &caller)
            {
                channel->notificationPlayStateChanged(caller, playState);
            }
        }
    }
    else if (&caller != &_sonosApi)
    {
        // called from master, take over play state
        bool playing = playState == SonosApiPlayState::Playing || playState == SonosApiPlayState::Transitioning;
        if (playing != (boolean)KoSON_CHPlayFeedback.value(DPT_Start))
            KoSON_CHPlayFeedback.value(playing, DPT_Start);
    }
}

void SonosChannel::notificationGroupCoordinatorChanged(SonosApi& caller)
{
    auto groupCoordinator = _sonosApi.findGroupCoordinator();
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

void SonosChannel::notificationTrackChanged(SonosApi& caller, SonosTrackInfo& trackInfo)
{
    uint8_t sourceNumber = 0;
    if (!trackInfo.uri.isEmpty())
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
                        auto groupCoordinator = _sonosApi.findGroupCoordinator();
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
                        auto groupCoordinator = _sonosApi.findGroupCoordinator();
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
            _volumeController.setVolume(volume);
            break;
        }
        case SON_KoCHVolumeRelativ:
        {
            bool increase = ko.value(DPT_Step);
            auto volume = increase ? (int8_t)ParamSON_CHRelativVolumeStep : -(int8_t)ParamSON_CHRelativVolumeStep;
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
            auto volume = increase ? (int8_t)ParamSON_CHGroupRelativVolumeStep : -(int8_t)ParamSON_CHGroupRelativVolumeStep;
            logDebugP("Set volume relative %d", volume);
            auto groupCoordinator = _sonosApi.findGroupCoordinator();
            if (groupCoordinator != nullptr)
                groupCoordinator->setGroupVolumeRelative(volume);
            break;
        }
        case SON_KoCHGroupMute:
        {
            boolean mute = ko.value(DPT_Switch);
            logDebugP("Set group mute %d", mute);
            auto groupCoordinator = _sonosApi.findGroupCoordinator();
            if (groupCoordinator != nullptr)
                groupCoordinator->setGroupMute(mute);
            break;
        }
        case SON_KoCHPlay:
        {
            boolean play = ko.value(DPT_Switch);
            logDebugP("Set play %d", play);
            auto groupCoordinator = _sonosApi.findGroupCoordinator();
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
            auto groupCoordinator = _sonosApi.findGroupCoordinator();
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
                    groupCoordinator->playMusicLibraryDirectory(uri);
                    break;
                }
                case 5: // Line In
                {
                    if (groupCoordinator != &_sonosApi)
                    {
                        groupCoordinator->delegateGroupCoordinationTo(&_sonosApi, false);
                    }
                    _sonosApi.playLineIn();
                    break;
                }
                case 6: // TV In
                {
                    if (groupCoordinator != &_sonosApi)
                    {
                        groupCoordinator->delegateGroupCoordinationTo(&_sonosApi, false);
                    }
                    _sonosApi.playTVIn();
                    break;
                }
                case 7: // Sonos playlist
                {
                    const char* uri = (const char*)ParamSONSRC_CHSourceUri;
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
                playNotification(notification);
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
                auto groupCoordinator = sonosChannel->_sonosApi.findGroupCoordinator();
                _sonosApi.joinToGroupCoordinator(groupCoordinator);
            }
        }
    }
    if (channelNumber == 0)
    {
        if (!delegateCoordination(false))
            _sonosApi.unjoin();
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
            _groupVolumeController.setVolume(value);
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
    else if (cmd.rfind("treb ", 0) == 0)
    {
        Serial.println();
        int value = atoi(cmd.c_str() + 5);
        if (value < -10 || value > 10)
            Serial.printf("Invalid treble %d\r\n", value);
        else
            _sonosApi.setTreble(value);
    }
    else if (cmd == "treb")
    {
        Serial.println();
        Serial.println(_sonosApi.getTreble());
    }
    else if (cmd.rfind("bass ", 0) == 0)
    {
        Serial.println();
        int value = atoi(cmd.c_str() + 5);
        if (value < -10 || value > 10)
            Serial.printf("Invalid bass %d\r\n", value);
        else
            _sonosApi.setBass(value);
    }
    else if (cmd == "bass")
    {
        Serial.println();
        Serial.println(_sonosApi.getBass());
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
    else if (cmd.rfind("ldn ", 0) == 0)
    {
        Serial.println();
        bool loudness = cmd.substr(4) == "1";
        _sonosApi.setLoudness(loudness);
    }
    else if (cmd == "ldn")
    {
        Serial.println();
        Serial.println(_sonosApi.getLoudness() ? "1" : "0");
    }
    else if (cmd.rfind("led ", 0) == 0)
    {
        Serial.println();
        bool on = cmd.substr(4) == "1";
        _sonosApi.setStatusLight(on);
    }
    else if (cmd == "led")
    {
        Serial.println();
        Serial.println(_sonosApi.getStatusLight() ? "1" : "0");
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
        _sonosApi.delegateGroupCoordinationTo(&(channel->_sonosApi), true);
    }
    else if (cmd.rfind("join ", 0) == 0)
    {
        Serial.println();
        auto targetChannel = atoi(cmd.substr(5).c_str());
        joinChannel(targetChannel);
    }
    else if (cmd.rfind("noti ", 0) == 0)
    {
        Serial.println();
        auto notificationNr = atoi(cmd.substr(5).c_str());
        playNotification(notificationNr);
    }
    else if (cmd.rfind("src ", 0) == 0)
    {
        Serial.println();
        auto src = atoi(cmd.substr(4).c_str());
        auto ko = KoSON_CHSourceNumber;
        ko.valueNoSend(src, DPT_Value_1_Ucount);
        processInputKo(ko);
    }
    else if (cmd.rfind("pl ", 0) == 0)
    {
        auto playList = cmd.substr(2);
        _sonosApi.playSonosPlaylist(playList.c_str());
    }
    else if (cmd == "test1")
    {
        _sonosApi.playInternetRadio("https://orf-live.ors-shoutcast.at/wie-q2a.m3u", "Radio Wien");
    }
    else if (cmd == "test2")
    {
        _sonosApi.playMusicLibraryDirectory("//192.168.0.1/Share/Storage/Musik/Violent Femmes/3");
    }
    else if (cmd == "test3")
    {
        _sonosApi.playMusicLibraryDirectory("//192.168.0.1/Share/Storage/Musik/Whippersnapper/Stories/");
    }
    else if (cmd == "test4")
    {
        _sonosApi.playMusicLibraryFile("//192.168.0.1/Share/Storage/Musik/Violent%20Femmes/3/01%20-%20Violent%20Femmes%20-%20Nightmares.mp3");
    }
    else if (cmd == "test5")
    {
        _sonosApi.stop();
    }
    else
        return false;
    return true;
}

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
            _playNotification = new SonosApiPlayNotification(_sonosApi.getSpeakerIP(), (const char*)ParamSON_NotificationUrl1, ParamSON_NotificationVolume1, _sonosApi.getUID());
            break;
        case 2:
            _playNotification = new SonosApiPlayNotification(_sonosApi.getSpeakerIP(), (const char*)ParamSON_NotificationUrl2, ParamSON_NotificationVolume2, _sonosApi.getUID());
            break;
        case 3:
            _playNotification = new SonosApiPlayNotification(_sonosApi.getSpeakerIP(), (const char*)ParamSON_NotificationUrl3, ParamSON_NotificationVolume3, _sonosApi.getUID());
            break;
        case 4:
            _playNotification = new SonosApiPlayNotification(_sonosApi.getSpeakerIP(), (const char*)ParamSON_NotificationUrl4, ParamSON_NotificationVolume4, _sonosApi.getUID());
            break;
    }
}

void SonosChannel::joinNextPlayingGroup()
{
    delegateCoordination(true);

    auto groupCoordinator = _sonosApi.findNextPlayingGroupCoordinator();
    if (groupCoordinator != nullptr)
    {
        logDebugP("Join with %s", groupCoordinator->getSpeakerIP().toString().c_str());
        _sonosApi.joinToGroupCoordinator(groupCoordinator);
    }
    else
        logDebugP("No next playing group found");
}

bool SonosChannel::delegateCoordination(bool rejoinGroup)
{
    auto currentGroupCoordinator = _sonosApi.findGroupCoordinator(true);
    if (currentGroupCoordinator == &_sonosApi)
    {
        // delegate to other participant of the group
        auto firstParticpant = _sonosApi.findFirstParticipant(true);
        if (firstParticpant != nullptr)
        {
            _sonosApi.delegateGroupCoordinationTo(firstParticpant, rejoinGroup);
            return true;
        }
    }
    return false;
}
