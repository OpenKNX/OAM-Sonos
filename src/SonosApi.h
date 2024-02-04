#pragma once
#include "OpenKNX.h"
#include "WiFi.h"
#include <MicroXPath.h>
#include <MicroXPath_P.h>
#include <ESPAsyncWebServer.h>

enum SonosApiPlayState : byte
{
  Unkown,
  Stopped, // STOPPED
  Transitioning, // TRANSITIONING
  Playing, // PLAYING
  Paused_Playback, // PAUSED_PLAYBACK
};

enum SonosApiPlayMode : byte
{
  Normal = 0, // NORMAL
  Repeat = 1, // REPEAT_ONE || REPEAT_ALL || SHUFFLE_REPEAT_ONE || SHUFFLE_NOREPEAT
  RepeatModeAll = 2, // REPEAT_ALL || SHUFFLE
  Shuffle = 4 // SHUFFLE_NOREPEAT || SHUFFLE_NOREPEAT || SHUFFLE || SHUFFLE_REPEAT_ONE
};

class SonosTrackInfo
{
  public:
    uint16_t trackNumber;
	  uint32_t duration;
	  String uri;
    String metadata;
};

class CurrentSonsTrackInfo : public SonosTrackInfo
{
  public:
	  uint32_t position;
};

class SonosApi;

class SonosApiNotificationHandler
{
  public:
    virtual void notificationVolumeChanged(SonosApi& caller, uint8_t volume) {}
    virtual void notificationMuteChanged(SonosApi& caller, boolean mute) {}
    virtual void notificationGroupVolumeChanged(SonosApi& caller, uint8_t volume) {};
    virtual void notificationGroupMuteChanged(SonosApi& caller, boolean mute) {};
    virtual void notificationPlayStateChanged(SonosApi& caller, SonosApiPlayState playState) {};
    virtual void notificationPlayModeChanged(SonosApi& caller, SonosApiPlayMode playMode) {};
    virtual void notificationTrackChanged(SonosApi& caller, SonosTrackInfo* trackInfo) {};
    virtual void notificationGroupCoordinatorChanged(SonosApi& caller) {};
};

class NotificationRestoreData
{
  public:
    CurrentSonsTrackInfo trackInfo;
    SonosApiPlayState playState;
    unsigned long lastChecked = millis();
};


class ParameterBuilder
{
    Stream* _stream;
    size_t _length;
  public:
    size_t length();
    ParameterBuilder(Stream* stream);
    void AddParameter(const char* name, const char* value1, const char* value2 = nullptr);
    void AddParameter(const char* name, int32_t value);
};

class SonosApi : private AsyncWebHandler
{
    static std::vector<SonosApi*> AllSonosApis;
    const static uint32_t _subscriptionTimeInSeconds = 600;
    String _uuid;
    String _groupCoordinatorUuid;
    SonosApi* _groupCoordinatorCached = nullptr;
    IPAddress _speakerIP;
    uint16_t _channelIndex;
    uint32_t _renderControlSeq = 0;
    unsigned long _subscriptionTime = 0;
    SonosApiNotificationHandler* _notificationHandler = nullptr;
    NotificationRestoreData* _notificationRestoreData = nullptr;
  
    const std::string logPrefix()
    {
      return "Sonos.API";
    }
    static uint32_t formatedTimestampToSeconds(char* durationAsString);
    static void wifiClient_xPath(MicroXPath_P& xPath, WiFiClient& wifiClient, PGM_P* path, uint8_t pathSize, char* resultBuffer, size_t resultBufferSize);

    void restorePlayState();

    typedef std::function<void(ParameterBuilder&)> TParameterBuilderFunction;
    void writeSoapHttpCall(Stream& stream, const char* soapUrl, const char* soapAction, const char* action, TParameterBuilderFunction parameterFunction);
    void writeSubscribeHttpCall(Stream& stream, const char* soapUrl);
    
    SonosApiPlayState getPlayStateFromString(const char* value);
    SonosApiPlayMode getPlayModeFromString(const char* value);
    const char* getPlayModeString(SonosApiPlayMode playMode);

    typedef std::function<void(WiFiClient&)> THandlerWifiResultFunction;
    int postAction(const char* soapUrl, const char* soapAction, const char* action, TParameterBuilderFunction parameterFunction = nullptr, THandlerWifiResultFunction wifiResultFunction = nullptr);
    int subscribeEvents(const char* soapUrl);
    void subscribeAll();

    // AsyncWebHandler
    bool canHandle(AsyncWebServerRequest *request) override final;
    void handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) override final;
   
  public:
    ~SonosApi();
    String& getUID();
    static String getUID(IPAddress ipAddress);

    IPAddress& getSpeakerIP();
    void init(AsyncWebServer* webServer, IPAddress speakerIP);
    void setCallback(SonosApiNotificationHandler* notificationHandler);
    void loop();
    void setVolume(uint8_t volume);
    void setVolumeRelative(int8_t volume);
    uint8_t getVolume();
    void setMute(boolean mute);
    boolean getMute();
    void setGroupVolume(uint8_t volume);
    void setGroupVolumeRelative(int8_t volume);
    uint8_t getGroupVolume();
    void setGroupMute(boolean mute);
    boolean getGroupMute();
    SonosApiPlayState getPlayState();
    void play();
    void pause();
    void next();
    void previous();
    void gotTrack(uint32_t trackNumber);
    SonosApiPlayMode getPlayMode();
    void setPlayMode(SonosApiPlayMode playMode);
    const CurrentSonsTrackInfo getTrackInfo();
    SonosApi* findGroupCoordinator(bool cached = false);
    SonosApi* findNextPlayingGroupCoordinator();
    void setAVTransportURI(const char* schema, const char* uri, const char* metadata = nullptr);
    void joinToGroupCoordinator(SonosApi* coordinator);
    void joinToGroupCoordinator(const char* uid);
    void unjoin();
    SonosApi* findFirstParticipant(bool cached = false);
    void delegateGroupCoordinationTo(SonosApi* sonosApi, bool rejoinGroup);
    void delegateGroupCoordinationTo(const char* uid, bool rejoinGroup);
    void playNotification(const char* schema, const char* uri, const char* metadata = nullptr);
    void gotoTrack(uint32_t trackNumber);
    void playSound();
};